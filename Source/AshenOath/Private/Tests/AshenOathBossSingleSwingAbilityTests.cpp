#if WITH_DEV_AUTOMATION_TESTS

#include "Characters/AshenOathBossCharacter.h"

#include "AI/AshenOathBossDecisionComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/AshenOathAttributeSet.h"
#include "AbilitySystem/Data/AshenOathMeleeActionData.h"
#include "AbilitySystem/Data/AshenOathBossDashSwingActionData.h"
#include "Actions/CombatMeleeComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffect.h"
#include "GameplayTags/AshenOathGameplayTags.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAshenOathBossSingleSwingAbilityLifecycleTest,
	"AshenOath.Combat.Ability.BossSingleSwingLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FAshenOathBossSingleSwingAbilityLifecycleTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	WorldContext.SetCurrentWorld(World);

	auto CleanupWorld = [World]()
	{
		World->EndPlay(EEndPlayReason::Quit);
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	};

	FURL URL;
	World->InitializeActorsForPlay(URL);
	World->BeginPlay();

	UClass* BossClass = LoadClass<AAshenOathBossCharacter>(
		nullptr,
		TEXT("/Game/AshenOath/Characters/BP_AshenOathBoss.BP_AshenOathBoss_C")
	);
	AAshenOathBossCharacter* Boss = BossClass
		? World->SpawnActor<AAshenOathBossCharacter>(BossClass)
		: nullptr;

	TestNotNull(TEXT("Boss Blueprint loads"), Boss);
	if (!Boss)
	{
		CleanupWorld();
		return false;
	}

	const UAshenOathBossDashSwingActionData* DashAction =
		LoadObject<UAshenOathBossDashSwingActionData>(
			nullptr,
			TEXT("/Game/AshenOath/CombatData/Boss/DA_Boss_DashSwing.DA_Boss_DashSwing")
		);
	TestNotNull(TEXT("Boss DashSwing ActionData loads"), DashAction);
	if (DashAction)
	{
		TestTrue(TEXT("DashSwing stops at a reachable swing distance"),
			DashAction->StopDistance > 0.0f && DashAction->StopDistance <= 300.0f);
		TestTrue(TEXT("DashSwing increases movement speed"),
			DashAction->DashSpeedMultiplier > 1.0f);
	}

	UAshenOathMeleeActionData* TestActionData = NewObject<UAshenOathMeleeActionData>(Boss);
	TestActionData->Montage = LoadObject<UAnimMontage>(
		nullptr,
		TEXT("/Game/ParagonSevarog/Characters/Heroes/Sevarog/Animations/Swing1_Medium_Montage.Swing1_Medium_Montage")
	);
	TestActionData->DamageEffect = LoadClass<UGameplayEffect>(
		nullptr,
		TEXT("/Game/AshenOath/AbilitySystem/Effects/Boss/GE_Boss_SingleSwing_Damage.GE_Boss_SingleSwing_Damage_C")
	);
	TestActionData->MeleeTraceRadius = 20.0f;

	USkeletalMeshComponent* Mesh = Boss->GetMesh();
	if (Mesh &&
		Mesh->DoesSocketExist(TEXT("root_weapon_r")) &&
		Mesh->DoesSocketExist(TEXT("root_weapon_end_r")))
	{
		TestActionData->MeleeTraceBones.Add(TEXT("root_weapon_r"));
		TestActionData->MeleeTraceBones.Add(TEXT("root_weapon_end_r"));
	}

	TestNotNull(TEXT("Sevarog test Montage loads"), TestActionData->Montage.Get());
	TestNotNull(TEXT("Test damage Effect loads"), TestActionData->DamageEffect.Get());
	TestEqual(TEXT("Boss test configuration has two valid trace bones"),
		TestActionData->MeleeTraceBones.Num(), 2);

	Boss->GrantSingleSwingForTesting(TestActionData);
	Boss->DispatchBeginPlay();

	UAbilitySystemComponent* AbilitySystem = Boss->GetAbilitySystemComponent();
	UCombatMeleeComponent* Melee = Boss->FindComponentByClass<UCombatMeleeComponent>();
	TestNotNull(TEXT("Boss ASC is ready"), AbilitySystem);
	TestNotNull(TEXT("Boss owns CombatMelee"), Melee);

	if (!AbilitySystem || !Melee)
	{
		CleanupWorld();
		return false;
	}

	FGameplayAbilitySpec* SingleSwingSpec = nullptr;
	for (FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
	{
		if (Spec.Ability && Spec.Ability->GetAssetTags().HasTagExact(
			AshenOathGameplayTags::Ability_Action_BossSingleSwing))
		{
			SingleSwingSpec = &Spec;
			break;
		}
	}

	TestNotNull(TEXT("Boss grants the configured single-swing Ability"), SingleSwingSpec);
	const UAshenOathMeleeActionData* ActionData = SingleSwingSpec
		? Cast<UAshenOathMeleeActionData>(SingleSwingSpec->SourceObject.Get())
		: nullptr;
	TestNotNull(TEXT("Single-swing Spec keeps ActionData as its source"), ActionData);

	if (!SingleSwingSpec || !ActionData)
	{
		CleanupWorld();
		return false;
	}

	TestTrue(TEXT("Single-swing activation request is accepted"), Boss->RequestSingleSwing());
	TestFalse(TEXT("A second request is rejected while the swing is active"), Boss->RequestSingleSwing());
	TestTrue(TEXT("Single-swing Ability remains active during its Montage"), SingleSwingSpec->IsActive());
	TestTrue(TEXT("A successful single swing owns a melee session"), Melee->HasActiveSession());

	int32 EndNotificationCount = 0;
	bool bLastEndWasCancelled = false;
	const FDelegateHandle EndNotificationHandle = Boss->OnSingleSwingEnded().AddLambda(
		[&EndNotificationCount, &bLastEndWasCancelled](const bool bWasCancelled)
		{
			++EndNotificationCount;
			bLastEndWasCancelled = bWasCancelled;
		}
	);

	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	const FAnimMontageInstance* MontageInstance = AnimInstance
		? AnimInstance->GetActiveInstanceForMontage(ActionData->Montage)
		: nullptr;
	TestNotNull(TEXT("The active single swing owns its Montage instance"), MontageInstance);

	if (!MontageInstance)
	{
		CleanupWorld();
		return false;
	}

	constexpr int32 NotifyInstanceId = 401;
	Melee->BeginHitWindowFromAnimation(
		Mesh,
		ActionData->Montage,
		MontageInstance->GetInstanceID(),
		NotifyInstanceId
	);
	TestTrue(TEXT("A matching animation signal opens the Boss hit window"),
		Melee->HasActiveHitWindow());

	Boss->CancelSingleSwing();

	TestFalse(TEXT("Cancellation ends the single-swing Ability"),
		SingleSwingSpec->IsActive());
	TestEqual(TEXT("The Boss reports one completion for the accepted request"),
		EndNotificationCount, 1);
	TestTrue(TEXT("The completion reports cancellation"), bLastEndWasCancelled);
	TestFalse(TEXT("Cancellation releases the Boss melee session"),
		Melee->HasActiveSession());
	TestFalse(TEXT("Cancellation closes the Boss hit window"),
		Melee->HasActiveHitWindow());

	int32 RequestEndCount = 0;
	FAshenOathBossAttackRequestHandle EndedRequest;
	bool bLastRequestWasCancelled = false;
	const FDelegateHandle RequestEndHandle = Boss->OnBossAttackEnded().AddLambda(
		[&RequestEndCount, &EndedRequest, &bLastRequestWasCancelled](
			FAshenOathBossAttackRequestHandle Request, bool bWasCancelled)
		{
			++RequestEndCount;
			EndedRequest = Request;
			bLastRequestWasCancelled = bWasCancelled;
		}
	);

	const FAshenOathBossAttackStartResult FirstRequest =
		Boss->RequestBossAttack(EAshenOathBossAttackType::SingleSwing);
	TestEqual(TEXT("A direct request runs the available swing"),
		FirstRequest.State, EAshenOathBossAttackStartState::Running);
	TestTrue(TEXT("A running request has an identity"),
		FirstRequest.RequestHandle.IsValid());
	TestFalse(TEXT("A different request cannot cancel the active swing"),
		Boss->CancelBossAttack({FirstRequest.RequestHandle.Value + 1}));
	TestTrue(TEXT("The unmatched cancellation leaves the swing active"),
		SingleSwingSpec->IsActive());
	TestTrue(TEXT("The owner can cancel its request"),
		Boss->CancelBossAttack(FirstRequest.RequestHandle));
	TestEqual(TEXT("The owned request ends exactly once"), RequestEndCount, 1);
	TestTrue(TEXT("The end notification identifies the first request"),
		EndedRequest == FirstRequest.RequestHandle);
	TestTrue(TEXT("Explicit cancellation reports failure to the task"),
		bLastRequestWasCancelled);

	const FAshenOathBossAttackStartResult SecondRequest =
		Boss->RequestBossAttack(EAshenOathBossAttackType::SingleSwing);
	TestEqual(TEXT("The next swing receives a new running request"),
		SecondRequest.State, EAshenOathBossAttackStartState::Running);
	TestFalse(TEXT("An expired request cannot cancel the next swing"),
		Boss->CancelBossAttack(FirstRequest.RequestHandle));
	TestTrue(TEXT("The next swing remains active"), SingleSwingSpec->IsActive());
	Boss->CancelBossAttack(SecondRequest.RequestHandle);
	TestEqual(TEXT("Each owned request emits one completion"), RequestEndCount, 2);
	Boss->OnBossAttackEnded().Remove(RequestEndHandle);

	ACharacter* DecisionTarget = World->SpawnActor<ACharacter>(
		ACharacter::StaticClass(),
		Boss->GetActorLocation() + FVector(900.0f, 0.0f, 0.0f),
		FRotator::ZeroRotator
	);
	TestNotNull(TEXT("Decision target spawns"), DecisionTarget);
	if (!DecisionTarget)
	{
		CleanupWorld();
		return false;
	}
	APlayerController* DecisionController = World->SpawnActor<APlayerController>();
	TestNotNull(TEXT("Decision controller spawns"), DecisionController);
	if (!DecisionController)
	{
		CleanupWorld();
		return false;
	}
	DecisionController->Possess(DecisionTarget);
	TestTrue(TEXT("The decision target is the current player pawn"),
		UGameplayStatics::GetPlayerPawn(World, 0) == DecisionTarget);

	TestTrue(TEXT("An eligible distant dash produces a decision"),
		Boss->ChooseFirstPhaseCombatIntent(DecisionTarget, 300.0f, 500.0f));
	TestEqual(TEXT("An eligible distant dash is selected by Utility"),
		Boss->GetPendingCombatIntent(), EAshenOathBossCombatIntent::Attack);
	float ApproachRange = 0.0f;
	Boss->ClearPendingCombatDecision();
	TestFalse(TEXT("An attack decision cannot be consumed as an approach decision"),
		Boss->TryConsumeApproachDecision(ApproachRange));

	DecisionTarget->SetActorLocation(Boss->GetActorLocation() + FVector(400.0f, 0.0f, 0.0f));
	TestTrue(TEXT("A target between melee and dash range produces an approach decision"),
		Boss->ChooseFirstPhaseCombatIntent(DecisionTarget, 300.0f, 500.0f));
	TestEqual(TEXT("The middle range selects approach"),
		Boss->GetPendingCombatIntent(), EAshenOathBossCombatIntent::Approach);
	TestTrue(TEXT("The approach decision can be consumed once"),
		Boss->TryConsumeApproachDecision(ApproachRange));
	TestEqual(TEXT("The distant approach ends at melee range"),
		ApproachRange, 300.0f);
	TestFalse(TEXT("The same approach decision cannot be consumed again"),
		Boss->TryConsumeApproachDecision(ApproachRange));

	DecisionTarget->SetActorLocation(Boss->GetActorLocation() + FVector(500.0f, 0.0f, 0.0f));
	TestTrue(TEXT("A target at the dash threshold produces an approach decision"),
		Boss->ChooseFirstPhaseCombatIntent(DecisionTarget, 300.0f, 500.0f));
	TestTrue(TEXT("The threshold decision approaches melee range"),
		Boss->TryConsumeApproachDecision(ApproachRange));
	TestEqual(TEXT("The melee approach uses the configured range"),
		ApproachRange, 300.0f);

	DecisionTarget->SetActorLocation(Boss->GetActorLocation() + FVector(900.0f, 0.0f, 0.0f));
	TestTrue(TEXT("An eligible dash has no maximum start range"),
		Boss->ChooseFirstPhaseCombatIntent(DecisionTarget, 300.0f, 500.0f));
	TestEqual(TEXT("The dash opportunity becomes an attack intent"),
		Boss->GetPendingCombatIntent(), EAshenOathBossCombatIntent::Attack);
	DecisionTarget->SetActorLocation(Boss->GetActorLocation() + FVector(400.0f, 0.0f, 0.0f));
	TestEqual(TEXT("A dash that lost its start range is not activated"),
		Boss->RequestSelectedCombatAttack().State, EAshenOathBossAttackStartState::Rejected);
	TestTrue(TEXT("Moving inside dash range still approaches from outside melee"),
		Boss->TryConsumeApproachDecision(ApproachRange));
	DecisionTarget->SetActorLocation(Boss->GetActorLocation() + FVector(900.0f, 0.0f, 0.0f));
	TestTrue(TEXT("The distant dash decision can be refreshed"),
		Boss->ChooseFirstPhaseCombatIntent(DecisionTarget, 300.0f, 500.0f));
	AbilitySystem->AddLooseGameplayTag(AshenOathGameplayTags::State_Staggered);
	const FAshenOathBossAttackStartResult RejectedDash = Boss->RequestSelectedCombatAttack();
	AbilitySystem->RemoveLooseGameplayTag(AshenOathGameplayTags::State_Staggered);
	TestEqual(TEXT("A rejected dash returns to a melee approach"),
		RejectedDash.State, EAshenOathBossAttackStartState::Rejected);
	TestTrue(TEXT("A rejected dash sets a safe approach fallback"),
		Boss->TryConsumeApproachDecision(ApproachRange));
	TestEqual(TEXT("The dash fallback approaches melee range"),
		ApproachRange, 300.0f);
	TestTrue(TEXT("A rejected dash did not start its cooldown"),
		Boss->ChooseFirstPhaseCombatIntent(DecisionTarget, 300.0f, 500.0f));
	TestEqual(TEXT("The rejected dash remains eligible"),
		Boss->GetPendingCombatIntent(), EAshenOathBossCombatIntent::Attack);
	Boss->ClearPendingCombatDecision();

	DecisionTarget->SetActorLocation(Boss->GetActorLocation() + FVector(150.0f, 0.0f, 0.0f));
	Boss->SetActorRotation(FRotator::ZeroRotator);
	TestTrue(TEXT("A nearby target produces a combat decision"),
		Boss->ChooseFirstPhaseCombatIntent(DecisionTarget, 300.0f, 500.0f));
	TestEqual(TEXT("A nearby target selects a melee attack"),
		Boss->GetPendingCombatIntent(), EAshenOathBossCombatIntent::Attack);
	UAshenOathBossDecisionComponent* DecisionComponent =
		Boss->FindComponentByClass<UAshenOathBossDecisionComponent>();
	TestNotNull(TEXT("Boss owns the Utility decision component"), DecisionComponent);
	if (!DecisionComponent || DecisionComponent->GetPendingDecisionForTesting().RankedAttacks.IsEmpty())
	{
		CleanupWorld();
		return false;
	}
	TestEqual(TEXT("SingleSwing wins at its preferred near distance"),
		DecisionComponent->GetPendingDecisionForTesting().RankedAttacks[0].AttackType,
		EAshenOathBossAttackType::SingleSwing);

	DecisionTarget->SetActorLocation(Boss->GetActorLocation() + FVector(210.0f, 0.0f, 0.0f));
	TestTrue(TEXT("A combo-range decision succeeds"),
		Boss->ChooseFirstPhaseCombatIntent(DecisionTarget, 300.0f, 500.0f));
	TestEqual(TEXT("Combo wins at its preferred distance"),
		DecisionComponent->GetPendingDecisionForTesting().RankedAttacks[0].AttackType,
		EAshenOathBossAttackType::Combo);

	DecisionTarget->SetActorLocation(Boss->GetActorLocation() + FVector(260.0f, 0.0f, 0.0f));
	TestTrue(TEXT("A charged-swing-range decision succeeds"),
		Boss->ChooseFirstPhaseCombatIntent(DecisionTarget, 300.0f, 500.0f));
	TestEqual(TEXT("ChargedSwing wins at its preferred distance"),
		DecisionComponent->GetPendingDecisionForTesting().RankedAttacks[0].AttackType,
		EAshenOathBossAttackType::ChargedSwing);

	DecisionTarget->SetActorLocation(Boss->GetActorLocation() + FVector(150.0f, 0.0f, 0.0f));
	TestTrue(TEXT("The near decision refreshes after range checks"),
		Boss->ChooseFirstPhaseCombatIntent(DecisionTarget, 300.0f, 500.0f));

	const EAshenOathBossAttackType FirstRankedAttack =
		DecisionComponent->GetPendingDecisionForTesting().RankedAttacks[0].AttackType;
	const float FrontFacingPenalty =
		DecisionComponent->GetPendingDecisionForTesting().RankedAttacks[0].FacingPenalty;
	Boss->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f));
	TestTrue(TEXT("Turning away still produces a decision"),
		Boss->ChooseFirstPhaseCombatIntent(DecisionTarget, 300.0f, 500.0f));
	for (const FAshenOathBossScoredAttack& Candidate :
		DecisionComponent->GetPendingDecisionForTesting().RankedAttacks)
	{
		if (Candidate.AttackType == FirstRankedAttack)
		{
			TestTrue(TEXT("Turning away increases the facing penalty"),
				Candidate.FacingPenalty > FrontFacingPenalty);
			break;
		}
	}
	Boss->SetActorRotation(FRotator::ZeroRotator);

	DecisionComponent->InitializeDecisionStream(1337);
	TestTrue(TEXT("The first seeded decision succeeds"),
		Boss->ChooseFirstPhaseCombatIntent(DecisionTarget, 300.0f, 500.0f));
	const auto FirstSeededRanking =
		DecisionComponent->GetPendingDecisionForTesting().RankedAttacks;
	DecisionComponent->InitializeDecisionStream(1337);
	TestTrue(TEXT("The repeated seeded decision succeeds"),
		Boss->ChooseFirstPhaseCombatIntent(DecisionTarget, 300.0f, 500.0f));
	const auto& SecondSeededRanking =
		DecisionComponent->GetPendingDecisionForTesting().RankedAttacks;
	TestEqual(TEXT("The same seed produces the same candidate count"),
		SecondSeededRanking.Num(), FirstSeededRanking.Num());
	for (int32 Index = 0; Index < FMath::Min(FirstSeededRanking.Num(), SecondSeededRanking.Num()); ++Index)
	{
		TestEqual(TEXT("The same seed preserves attack order"),
			SecondSeededRanking[Index].AttackType, FirstSeededRanking[Index].AttackType);
		TestEqual(TEXT("The same seed preserves the tie value"),
			SecondSeededRanking[Index].TieBreak, FirstSeededRanking[Index].TieBreak);
	}

	const EAshenOathBossAttackType SelectedAttack = SecondSeededRanking[0].AttackType;
	const float MaxStamina = AbilitySystem->GetNumericAttribute(
		UAshenOathAttributeSet::GetMaxStaminaAttribute());
	AbilitySystem->SetNumericAttributeBase(
		UAshenOathAttributeSet::GetStaminaAttribute(), MaxStamina);
	const FAshenOathBossAttackStartResult UtilityRequest =
		Boss->RequestSelectedCombatAttack();
	TestTrue(TEXT("The selected Utility attack is accepted"),
		UtilityRequest.State == EAshenOathBossAttackStartState::Running ||
		UtilityRequest.State == EAshenOathBossAttackStartState::Succeeded);
	if (UtilityRequest.State == EAshenOathBossAttackStartState::Running)
	{
		Boss->CancelBossAttack(UtilityRequest.RequestHandle);
	}

	TestTrue(TEXT("A new decision follows the accepted attack"),
		Boss->ChooseFirstPhaseCombatIntent(DecisionTarget, 300.0f, 500.0f));
	bool bSelectedAttackIsCoolingDown = true;
	for (const FAshenOathBossScoredAttack& Candidate :
		DecisionComponent->GetPendingDecisionForTesting().RankedAttacks)
	{
		bSelectedAttackIsCoolingDown &= Candidate.AttackType != SelectedAttack;
	}
	TestTrue(TEXT("An accepted attack is excluded during its cooldown"),
		bSelectedAttackIsCoolingDown);
	DecisionController->UnPossess();
	TestEqual(TEXT("A decision for a lost player pawn is no longer visible"),
		Boss->GetPendingCombatIntent(), EAshenOathBossCombatIntent::None);
	TestEqual(TEXT("A stale attack decision cannot activate a Boss Ability"),
		Boss->RequestSelectedCombatAttack().State, EAshenOathBossAttackStartState::Rejected);
	TestFalse(TEXT("A lost player pawn cannot produce a new decision"),
		Boss->ChooseFirstPhaseCombatIntent(DecisionTarget, 300.0f, 500.0f));
	DecisionController->Possess(DecisionTarget);

	TestActionData->StartSection = TEXT("MissingSection");
	TestFalse(TEXT("An invalid Boss start section rejects activation"),
		Boss->RequestSingleSwing());
	TestEqual(TEXT("A rejected swing emits no completion"),
		EndNotificationCount, 1);
	TestFalse(TEXT("A rejected swing creates no melee session"),
		Melee->HasActiveSession());

	Boss->OnSingleSwingEnded().Remove(EndNotificationHandle);

	CleanupWorld();
	return true;
}

#endif
