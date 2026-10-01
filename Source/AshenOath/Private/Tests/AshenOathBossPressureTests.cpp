#if WITH_DEV_AUTOMATION_TESTS

#include "AI/AshenOathBossDecisionComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/AshenOathAttributeSet.h"
#include "Characters/AshenOathBossCharacter.h"
#include "Components/StateTreeComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "GameplayTags/AshenOathGameplayTags.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAshenOathBossPressureTest,
	"AshenOath.AI.BossPressure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FAshenOathBossPressureTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	WorldContext.SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();
	// Complete the GameMode's usual start-play notification in this isolated world.
	World->GetWorldSettings()->NotifyBeginPlay();
	auto CleanupWorld = [World]()
	{
		World->EndPlay(EEndPlayReason::Quit);
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	};

	UClass* BossClass = LoadClass<AAshenOathBossCharacter>(
		nullptr, TEXT("/Game/AshenOath/Characters/BP_AshenOathBoss.BP_AshenOathBoss_C"));
	AAshenOathBossCharacter* Boss = BossClass ? World->SpawnActor<AAshenOathBossCharacter>(BossClass) : nullptr;
	ACharacter* Player = World->SpawnActor<ACharacter>(
		ACharacter::StaticClass(), FVector(170.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	if (!TestNotNull(TEXT("Configured Boss Blueprint spawns"), Boss) ||
		!TestNotNull(TEXT("Player target spawns"), Player) || !TestNotNull(TEXT("Player controller spawns"), Controller))
	{
		CleanupWorld();
		return false;
	}
	Controller->Possess(Player);
	// A synthetic world has no floor; keep the distance fixture stationary.
	Boss->GetCharacterMovement()->SetMovementMode(MOVE_None);
	Player->GetCharacterMovement()->SetMovementMode(MOVE_None);
	UAshenOathBossDecisionComponent* Decision = Boss->FindComponentByClass<UAshenOathBossDecisionComponent>();
	UAbilitySystemComponent* ASC = Boss->GetAbilitySystemComponent();
	UStateTreeComponent* Tree = Boss->FindComponentByClass<UStateTreeComponent>();
	if (!TestNotNull(TEXT("Decision component exists"), Decision) || !TestNotNull(TEXT("Boss ASC exists"), ASC) ||
		!TestNotNull(TEXT("Configured StateTree component exists"), Tree))
	{
		CleanupWorld();
		return false;
	}
	auto RefillStamina = [ASC]()
	{
		ASC->SetNumericAttributeBase(UAshenOathAttributeSet::GetStaminaAttribute(),
			ASC->GetNumericAttribute(UAshenOathAttributeSet::GetMaxStaminaAttribute()));
	};
	auto Choose = [Boss, Player]()
	{
		Player->SetActorLocation(Boss->GetActorLocation() + FVector(170.0f, 0.0f, 0.0f));
		Boss->SetActorRotation(FRotator::ZeroRotator);
		return Boss->ChooseFirstPhaseCombatIntent(Player, 250.0f, 500.0f, 1000.0f);
	};
	auto TickWorld = [World](const float DeltaSeconds)
	{
		// TickTaskManager visits each function once per GFrameCounter. This synchronous
		// test simulates separate engine frames rather than repeatedly ticking one frame.
		++GFrameCounter;
		World->Tick(LEVELTICK_All, DeltaSeconds);
	};
	Decision->InitializeDecisionStream(1337);
	TestFalse(TEXT("Pressure requires no background component tick"), Decision->IsComponentTickEnabled());
	Choose();
	ASC->AddLooseGameplayTag(AshenOathGameplayTags::State_Staggered);
	TestEqual(TEXT("A blocked attack is rejected"), Boss->RequestSelectedCombatAttack().State,
		EAshenOathBossAttackStartState::Rejected);
	ASC->RemoveLooseGameplayTag(AshenOathGameplayTags::State_Staggered);
	TestEqual(TEXT("Rejection consumes no pressure"), Decision->GetPressure(), 0.0f);

	int32 AcceptedCount = 0;
	for (int32 Attempt = 0; Attempt < 8 && !Decision->NeedsBreather(); ++Attempt)
	{
		RefillStamina();
		Choose();
		const float PreviousPressure = Decision->GetPressure();
		const FAshenOathBossAttackStartResult Result = Boss->RequestSelectedCombatAttack();
		if (!TestTrue(TEXT("The burst can start an eligible attack"),
			Result.State == EAshenOathBossAttackStartState::Running || Result.State == EAshenOathBossAttackStartState::Succeeded))
		{
			CleanupWorld();
			return false;
		}
		++AcceptedCount;
		const float AcceptedPressure = Decision->GetPressure();
		TestTrue(TEXT("Each accepted attack adds pressure"), AcceptedPressure > PreviousPressure);
		TickWorld(0.2f);
		TestEqual(TEXT("Attack execution does not release pressure"), Decision->GetPressure(), AcceptedPressure);
		if (Result.State == EAshenOathBossAttackStartState::Running)
		{
			Boss->CancelBossAttack(Result.RequestHandle);
		}
		TestEqual(TEXT("Cancellation does not refund accepted pressure"), Decision->GetPressure(), AcceptedPressure);
		TickWorld(1.0f);
		TestEqual(TEXT("Elapsed time outside Observe does not release pressure"), Decision->GetPressure(), AcceptedPressure);
	}
	TestTrue(TEXT("A finite burst reaches the budget limit"), Decision->NeedsBreather() && AcceptedCount <= 8);
	TestEqual(TEXT("Pressure is capped at the budget limit"), Decision->GetPressure(), 1.0f);
	Choose();
	TestEqual(TEXT("A full budget forces Observe even with eligible melee attacks"),
		Boss->GetPendingCombatIntent(), EAshenOathBossCombatIntent::Wait);
	TestEqual(TEXT("A full budget cannot request another attack"), Boss->RequestSelectedCombatAttack().State,
		EAshenOathBossAttackStartState::Rejected);
	Player->SetActorLocation(Boss->GetActorLocation() + FVector(700.0f, 0.0f, 0.0f));
	Boss->ChooseFirstPhaseCombatIntent(Player, 250.0f, 500.0f, 1000.0f);
	TestEqual(TEXT("Switching to Dash cannot bypass the breather"), Boss->GetPendingCombatIntent(), EAshenOathBossCombatIntent::Wait);

	Decision->ReleasePressureWhileObserving(1.0f);
	Choose();
	TestTrue(TEXT("Rest stays latched below the upper threshold"), Decision->NeedsBreather());
	TestEqual(TEXT("One second of rest cannot immediately restart the burst"),
		Boss->GetPendingCombatIntent(), EAshenOathBossCombatIntent::Wait);
	Decision->ReleasePressureWhileObserving(1.2f);
	Choose();
	TestFalse(TEXT("Crossing the lower threshold permits a new burst"), Decision->NeedsBreather());
	TestEqual(TEXT("An eligible attack resumes after sufficient observation"),
		Boss->GetPendingCombatIntent(), EAshenOathBossCombatIntent::Attack);
	Decision->ReleasePressureWhileObserving(5.0f);
	TestEqual(TEXT("Long observation cannot produce negative pressure"), Decision->GetPressure(), 0.0f);

#if WITH_GAMEPLAY_DEBUGGER
	// Active state names are available in the engine's GameplayDebugger builds.
	// Exercise the actual asset and task wiring, including cancellation and Recovery.
	Decision->InitializeDecisionStream(1337);
	Tree->StartLogic();
	TestTrue(TEXT("The configured combat tree starts"), Tree->IsRunning());
	bool bSawBudgetFull = false;
	bool bSawObserve = false;
	bool bSawResumedAttack = false;
	for (int32 Step = 0; Step < 600 && !bSawResumedAttack; ++Step)
	{
		Player->SetActorLocation(Boss->GetActorLocation() + FVector(170.0f, 0.0f, 0.0f));
		RefillStamina();
		const TArray<FName> States = Tree->GetActiveStateNames();
		bSawBudgetFull |= Decision->NeedsBreather();
		if (States.Contains(FName(TEXT("Observe"))) && bSawBudgetFull)
		{
			bSawObserve = true;
		}
		if (States.Contains(FName(TEXT("First Phase Attack"))))
		{
			bSawResumedAttack = bSawObserve && !Decision->NeedsBreather();
			// Complete the test's burst without depending on visual montage ticking.
			// Accepted but interrupted attacks must still consume their budget.
			ASC->CancelAllAbilities();
		}
		const float BeforeTick = Decision->GetPressure();
		TickWorld(0.05f);
		if (States.Contains(FName(TEXT("Recovery"))))
		{
			TestTrue(TEXT("Recovery never drains the budget"), Decision->GetPressure() >= BeforeTick);
		}
	}
	TestTrue(TEXT("Actual StateTree exhausts its attack budget"), bSawBudgetFull);
	TestTrue(TEXT("Actual StateTree enters Observe to release pressure"), bSawObserve);
	TestTrue(TEXT("Actual StateTree resumes attacks after the lower threshold"), bSawResumedAttack);
	Tree->StopLogic(TEXT("Pressure test completed"));
#endif
	Controller->UnPossess();
	TestFalse(TEXT("A lost player cannot produce another decision"),
		Boss->ChooseFirstPhaseCombatIntent(Player, 250.0f, 500.0f, 1000.0f));
	Decision->InitializeDecisionStream(1337);
	TestEqual(TEXT("A new combat stream resets pressure"), Decision->GetPressure(), 0.0f);
	TestFalse(TEXT("A new combat stream resets the breather latch"), Decision->NeedsBreather());
	CleanupWorld();
	return true;
}

#endif
