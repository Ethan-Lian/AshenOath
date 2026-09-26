#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/AshenOathAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Characters/AshenOathBossCharacter.h"
#include "Characters/AshenOathPlayerCharacter.h"
#include "Damage/CombatDamageComponent.h"
#include "Death/CombatDeathComponent.h"
#include "Defense/CombatDefenseComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/AshenOathGameMode.h"
#include "GameplayEffect.h"
#include "GameplayTags/AshenOathGameplayTags.h"
#include "Misc/AutomationTest.h"
#include "Player/AshenOathPlayerController.h"

namespace AshenOathDeathAndMatchTests
{
	UWorld* CreateGameWorld()
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		FWorldContext& WorldContext =
			GEngine->CreateNewWorldContext(EWorldType::Game);
		WorldContext.SetCurrentWorld(World);

		FURL URL;
		World->InitializeActorsForPlay(URL);
		World->BeginPlay();
		return World;
	}

	void DestroyGameWorld(UWorld* World)
	{
		if (!World)
		{
			return;
		}

		World->EndPlay(EEndPlayReason::Quit);
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	}

	bool SpawnMatchActors(
		UWorld& World,
		AAshenOathPlayerController*& OutController,
		AAshenOathPlayerCharacter*& OutPlayer,
		AAshenOathBossCharacter*& OutBoss)
	{
		OutController = World.SpawnActor<AAshenOathPlayerController>();
		OutPlayer = World.SpawnActor<AAshenOathPlayerCharacter>();
		OutBoss = World.SpawnActor<AAshenOathBossCharacter>();

		if (!OutController || !OutPlayer || !OutBoss)
		{
			return false;
		}

		// Transient automation worlds do not consistently dispatch BeginPlay for
		// actors spawned after UWorld::BeginPlay. The production bindings and tag
		// configuration under test are established there, so mirror the normal
		// game-world lifecycle when the test world has not done it for us.
		for (AActor* Actor : {static_cast<AActor*>(OutController), static_cast<AActor*>(OutPlayer), static_cast<AActor*>(OutBoss)})
		{
			if (!Actor->HasActorBegunPlay())
			{
				Actor->DispatchBeginPlay();
			}
		}

		OutController->Possess(OutPlayer);
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAshenOathDeathLifecycleTest,
	"AshenOath.Combat.Lifecycle.Death",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FAshenOathDeathLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace AshenOathDeathAndMatchTests;

	UWorld* World = CreateGameWorld();
	AAshenOathPlayerController* PlayerController = nullptr;
	AAshenOathPlayerCharacter* Player = nullptr;
	AAshenOathBossCharacter* Boss = nullptr;

	TestTrue(
		TEXT("Match actors spawn"),
		SpawnMatchActors(*World, PlayerController, Player, Boss)
	);

	if (!PlayerController || !Player || !Boss)
	{
		DestroyGameWorld(World);
		return false;
	}

	UAbilitySystemComponent* PlayerAbilitySystem =
		Player->GetAbilitySystemComponent();
	UCombatDeathComponent* Death =
		Player->FindComponentByClass<UCombatDeathComponent>();
	UCombatDefenseComponent* Defense =
		Player->FindComponentByClass<UCombatDefenseComponent>();
	UCombatDamageComponent* Damage =
		Player->FindComponentByClass<UCombatDamageComponent>();

	TestNotNull(TEXT("Player owns an ASC"), PlayerAbilitySystem);
	TestNotNull(TEXT("Player owns CombatDeath"), Death);
	TestNotNull(TEXT("Player owns CombatDefense"), Defense);
	TestNotNull(TEXT("Player owns CombatDamage"), Damage);

	if (!PlayerAbilitySystem || !Death || !Defense || !Damage)
	{
		DestroyGameWorld(World);
		return false;
	}

	PlayerAbilitySystem->SetNumericAttributeBase(
		UAshenOathAttributeSet::GetMaxHealthAttribute(),
		100.0f
	);
	PlayerAbilitySystem->SetNumericAttributeBase(
		UAshenOathAttributeSet::GetHealthAttribute(),
		100.0f
	);
	TestTrue(TEXT("Death observation initializes"), Death->Initialize(PlayerAbilitySystem));

	int32 DeathEventCount = 0;
	const FDelegateHandle DeathHandle = Death->OnDeathStarted().AddLambda(
		[&DeathEventCount]()
		{
			++DeathEventCount;
		}
	);

	FGameplayTagContainer DefenseTags;
	DefenseTags.AddTag(AshenOathGameplayTags::State_Invulnerable);
	const FCombatDefenseWindowHandle DefenseHandle = Defense->BeginDodgeWindow(
		Player,
		DefenseTags,
		0.0f,
		1.0f,
		World->GetTimeSeconds(),
		0.1f,
		0.2f
	);
	TestTrue(TEXT("Pre-death defense window starts"), DefenseHandle.IsValid());
	TestTrue(TEXT("Pre-death defense window activates"), Defense->ActivateDodgeWindow(DefenseHandle));

	PlayerAbilitySystem->SetNumericAttributeBase(
		UAshenOathAttributeSet::GetHealthAttribute(),
		0.0f
	);

	TestTrue(TEXT("Zero health starts death"), Death->IsDeathStarted());
	TestEqual(TEXT("Death broadcasts once"), DeathEventCount, 1);
	TestEqual(
		TEXT("Death owns one terminal tag"),
		PlayerAbilitySystem->GetTagCount(AshenOathGameplayTags::State_Dead),
		1
	);
	TestFalse(TEXT("Death clears the defense window"), Defense->OwnsWindow(DefenseHandle));
	TestEqual(
		TEXT("Death removes defense-owned invulnerability"),
		PlayerAbilitySystem->GetTagCount(AshenOathGameplayTags::State_Invulnerable),
		0
	);

	PlayerAbilitySystem->SetNumericAttributeBase(
		UAshenOathAttributeSet::GetHealthAttribute(),
		50.0f
	);
	PlayerAbilitySystem->SetNumericAttributeBase(
		UAshenOathAttributeSet::GetHealthAttribute(),
		0.0f
	);
	TestEqual(TEXT("A dead actor cannot transition twice"), DeathEventCount, 1);
	TestEqual(
		TEXT("Repeated zero health does not duplicate the terminal tag"),
		PlayerAbilitySystem->GetTagCount(AshenOathGameplayTags::State_Dead),
		1
	);

	FCombatDamageAttempt DamageAttempt;
	DamageAttempt.SourceActor = Boss;
	DamageAttempt.DamageEffect = UGameplayEffect::StaticClass();
	DamageAttempt.HitTimeSeconds = World->GetTimeSeconds();
	TestEqual(
		TEXT("Terminal actors reject further damage"),
		Damage->ApplyDamageAttempt(DamageAttempt),
		ECombatDamageResult::Invalid
	);

	Death->OnDeathStarted().Remove(DeathHandle);
	DestroyGameWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAshenOathMatchOutcomeTest,
	"AshenOath.Combat.Lifecycle.MatchOutcome",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FAshenOathMatchOutcomeTest::RunTest(const FString& Parameters)
{
	using namespace AshenOathDeathAndMatchTests;

	for (const bool bBossDiesFirst : {false, true})
	{
		UWorld* World = CreateGameWorld();
		AAshenOathGameMode* GameMode =
			World->SpawnActor<AAshenOathGameMode>();
		AAshenOathPlayerController* PlayerController = nullptr;
		AAshenOathPlayerCharacter* Player = nullptr;
		AAshenOathBossCharacter* Boss = nullptr;

		TestNotNull(TEXT("Test world owns AshenOath GameMode"), GameMode);
		TestTrue(
			TEXT("Match actors spawn"),
			SpawnMatchActors(*World, PlayerController, Player, Boss)
		);

		if (!GameMode || !PlayerController || !Player || !Boss)
		{
			DestroyGameWorld(World);
			return false;
		}

		TestTrue(TEXT("Boss registers as active"), GameMode->RegisterBoss(Boss));

		int32 OutcomeEventCount = 0;
		EAshenOathMatchOutcome ObservedOutcome =
			EAshenOathMatchOutcome::InProgress;
		const FDelegateHandle OutcomeHandle =
			GameMode->OnMatchOutcomeChanged().AddLambda(
				[&OutcomeEventCount, &ObservedOutcome](
					const EAshenOathMatchOutcome Outcome)
				{
					++OutcomeEventCount;
					ObservedOutcome = Outcome;
				}
			);

		const EAshenOathMatchOutcome ExpectedOutcome = bBossDiesFirst
			? EAshenOathMatchOutcome::Victory
			: EAshenOathMatchOutcome::Defeat;

		if (bBossDiesFirst)
		{
			GameMode->ReportBossDeath(Boss);
			GameMode->ReportBossDeath(Boss);
			GameMode->ReportPlayerDeath(Player);
		}
		else
		{
			GameMode->ReportPlayerDeath(Player);
			GameMode->ReportPlayerDeath(Player);
			GameMode->ReportBossDeath(Boss);
		}

		TestEqual(TEXT("First terminal report owns the outcome"), GameMode->GetMatchOutcome(), ExpectedOutcome);
		TestEqual(TEXT("Outcome listener observes the committed value"), ObservedOutcome, ExpectedOutcome);
		TestEqual(TEXT("Outcome broadcasts exactly once"), OutcomeEventCount, 1);

		GameMode->OnMatchOutcomeChanged().Remove(OutcomeHandle);
		DestroyGameWorld(World);
	}

	return true;
}

#endif
