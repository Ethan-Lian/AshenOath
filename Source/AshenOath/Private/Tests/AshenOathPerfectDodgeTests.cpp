#if WITH_DEV_AUTOMATION_TESTS

#include "Characters/AshenOathBossCharacter.h"
#include "Characters/AshenOathPlayerCharacter.h"

#include "AbilitySystemComponent.h"
#include "Damage/CombatDamageComponent.h"
#include "Defense/CombatDefenseComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffect.h"
#include "GameplayTags/AshenOathGameplayTags.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAshenOathPerfectDodgeDecisionTest,
	"AshenOath.Combat.Damage.PerfectDodgeDecision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FAshenOathPerfectDodgeDecisionTest::RunTest(const FString& Parameters)
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

	AAshenOathPlayerCharacter* Player =
		World->SpawnActor<AAshenOathPlayerCharacter>();
	APlayerController* PlayerController = World->SpawnActor<APlayerController>();
	AAshenOathBossCharacter* Source =
		World->SpawnActor<AAshenOathBossCharacter>();

	TestNotNull(TEXT("Player spawns"), Player);
	TestNotNull(TEXT("Player controller spawns"), PlayerController);
	TestNotNull(TEXT("Damage source spawns"), Source);

	if (!Player || !PlayerController || !Source)
	{
		CleanupWorld();
		return false;
	}

	PlayerController->Possess(Player);

	UAbilitySystemComponent* SourceAbilitySystem = Source->GetAbilitySystemComponent();
	UCombatDefenseComponent* Defense =
		Player->FindComponentByClass<UCombatDefenseComponent>();
	UCombatDamageComponent* Damage =
		Player->FindComponentByClass<UCombatDamageComponent>();

	TestNotNull(TEXT("Source owns an ASC"), SourceAbilitySystem);
	TestNotNull(TEXT("Player owns CombatDefense"), Defense);
	TestNotNull(TEXT("Player owns CombatDamage"), Damage);

	if (!SourceAbilitySystem || !Defense || !Damage)
	{
		CleanupWorld();
		return false;
	}

	SourceAbilitySystem->InitAbilityActorInfo(Source, Source);
	Damage->ConfigureInvulnerabilityTag(AshenOathGameplayTags::State_Invulnerable);
	int32 ResolvedResultCount = 0;
	int32 PerfectDodgeResultCount = 0;
	const FDelegateHandle DamageResolvedHandle = Damage->OnDamageResolved().AddLambda(
		[&ResolvedResultCount, &PerfectDodgeResultCount](
			const FCombatDamageAttempt&,
			const ECombatDamageResult Result)
		{
			++ResolvedResultCount;
			if (Result == ECombatDamageResult::PerfectDodge)
			{
				++PerfectDodgeResultCount;
			}
		}
	);

	FGameplayTagContainer WindowTags;
	WindowTags.AddTag(AshenOathGameplayTags::State_Invulnerable);

	const double FirstExecutionStart = World->GetTimeSeconds();
	const FCombatDefenseWindowHandle InvalidWindow = Defense->BeginDodgeWindow(
		Player,
		WindowTags,
		0.0f,
		1.0f,
		FirstExecutionStart,
		0.0f,
		1.0f
	);
	TestFalse(
		TEXT("Perfect dodge cannot cover the entire ordinary dodge window"),
		InvalidWindow.IsValid()
	);

	const FCombatDefenseWindowHandle FirstWindow = Defense->BeginDodgeWindow(
		Player,
		WindowTags,
		0.0f,
		1.0f,
		FirstExecutionStart,
		0.1f,
		0.2f
	);
	TestTrue(TEXT("A proper perfect-dodge subset is accepted"), FirstWindow.IsValid());
	TestTrue(TEXT("The defense window activates"), Defense->ActivateDodgeWindow(FirstWindow));

	FCombatDamageAttempt Attempt;
	Attempt.DamageEffect = UGameplayEffect::StaticClass();
	Attempt.SourceActor = Source;
	Attempt.bCanTriggerPerfectDodge = true;

	Attempt.HitTimeSeconds = FirstExecutionStart + 0.05;
	TestEqual(
		TEXT("Eligible hits before the perfect interval remain ordinary dodges"),
		Damage->ApplyDamageAttempt(Attempt),
		ECombatDamageResult::DodgeInvulnerable
	);

	Attempt.HitTimeSeconds = FirstExecutionStart + 0.15;
	TestEqual(
		TEXT("The first eligible hit inside the perfect interval is consumed"),
		Damage->ApplyDamageAttempt(Attempt),
		ECombatDamageResult::PerfectDodge
	);

	Attempt.HitTimeSeconds = FirstExecutionStart + 0.16;
	TestEqual(
		TEXT("A second eligible hit in the same dodge is only an ordinary dodge"),
		Damage->ApplyDamageAttempt(Attempt),
		ECombatDamageResult::DodgeInvulnerable
	);

	Defense->EndDodgeWindow(FirstWindow);

	const double SecondExecutionStart = World->GetTimeSeconds();
	const FCombatDefenseWindowHandle SecondWindow = Defense->BeginDodgeWindow(
		Player,
		WindowTags,
		0.0f,
		1.0f,
		SecondExecutionStart,
		0.1f,
		0.2f
	);
	TestTrue(TEXT("A later dodge owns a fresh perfect-dodge budget"), SecondWindow.IsValid());
	TestTrue(TEXT("The later defense window activates"), Defense->ActivateDodgeWindow(SecondWindow));

	Attempt.bCanTriggerPerfectDodge = false;
	Attempt.HitTimeSeconds = SecondExecutionStart + 0.15;
	TestEqual(
		TEXT("An ineligible hit inside the perfect interval remains an ordinary dodge"),
		Damage->ApplyDamageAttempt(Attempt),
		ECombatDamageResult::DodgeInvulnerable
	);

	Attempt.bCanTriggerPerfectDodge = true;
	Player->GetAbilitySystemComponent()->AddLooseGameplayTag(
		AshenOathGameplayTags::State_Invulnerable
	);
	TestEqual(
		TEXT("Independent invulnerability takes priority over perfect dodge"),
		Damage->ApplyDamageAttempt(Attempt),
		ECombatDamageResult::OtherInvulnerable
	);
	Player->GetAbilitySystemComponent()->RemoveLooseGameplayTag(
		AshenOathGameplayTags::State_Invulnerable
	);

	Defense->EndDodgeWindow(SecondWindow);
	TestFalse(
		TEXT("Ending the dodge removes its perfect-dodge ownership"),
		Defense->TryConsumePerfectDodge(
			AshenOathGameplayTags::State_Invulnerable,
			SecondExecutionStart + 0.15
		)
	);
	TestEqual(TEXT("Every classified rejection is observable"), ResolvedResultCount, 5);
	TestEqual(TEXT("Exactly one perfect dodge is observable"), PerfectDodgeResultCount, 1);
	Damage->OnDamageResolved().Remove(DamageResolvedHandle);

	CleanupWorld();
	return true;
}

#endif
