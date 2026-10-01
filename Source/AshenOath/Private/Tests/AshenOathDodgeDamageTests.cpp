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
	FAshenOathDodgeDamageTest,
	"AshenOath.Combat.Damage.DodgeInvulnerability",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FAshenOathDodgeDamageTest::RunTest(const FString& Parameters)
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
	const FDelegateHandle DamageResolvedHandle = Damage->OnDamageResolved().AddLambda(
		[&ResolvedResultCount](const FCombatDamageAttempt&, const ECombatDamageResult)
		{
			++ResolvedResultCount;
		}
	);

	FGameplayTagContainer WindowTags;
	WindowTags.AddTag(AshenOathGameplayTags::State_Invulnerable);
	const double ExecutionStart = World->GetTimeSeconds();
	const FCombatDefenseWindowHandle Window = Defense->BeginDodgeWindow(
		Player, WindowTags, 0.1f, 1.0f, ExecutionStart
	);
	TestTrue(TEXT("The dodge window is reserved"), Window.IsValid());
	TestTrue(TEXT("The dodge window activates"), Defense->ActivateDodgeWindow(Window));

	FCombatDamageAttempt Attempt;
	Attempt.DamageEffect = UGameplayEffect::StaticClass();
	Attempt.SourceActor = Source;

	// Match the configured float offset exactly when checking the inclusive boundary.
	const double WindowStart = ExecutionStart + static_cast<double>(0.1f);
	Attempt.HitTimeSeconds = WindowStart - 0.001;
	TestEqual(TEXT("Damage before the window is accepted"),
		Damage->ApplyDamageAttempt(Attempt), ECombatDamageResult::Applied);
	Attempt.HitTimeSeconds = WindowStart;
	TestEqual(TEXT("The beginning of the window rejects damage"),
		Damage->ApplyDamageAttempt(Attempt), ECombatDamageResult::DodgeInvulnerable);
	Attempt.HitTimeSeconds = ExecutionStart + 0.15;
	TestEqual(TEXT("Damage early in the window remains a normal dodge"),
		Damage->ApplyDamageAttempt(Attempt), ECombatDamageResult::DodgeInvulnerable);
	Attempt.HitTimeSeconds = ExecutionStart + 0.16;
	TestEqual(TEXT("Repeated hits remain invulnerable throughout the window"),
		Damage->ApplyDamageAttempt(Attempt), ECombatDamageResult::DodgeInvulnerable);
	Attempt.HitTimeSeconds = WindowStart + 1.0;
	TestEqual(TEXT("The end of the window accepts damage"),
		Damage->ApplyDamageAttempt(Attempt), ECombatDamageResult::Applied);

	Defense->EndDodgeWindow(Window);
	Attempt.HitTimeSeconds = ExecutionStart + 0.15;
	TestEqual(TEXT("Ending the dodge revokes its time-window protection"),
		Damage->ApplyDamageAttempt(Attempt), ECombatDamageResult::Applied);

	const FCombatDefenseWindowHandle NextWindow = Defense->BeginDodgeWindow(
		Player, WindowTags, 0.0f, 1.0f, ExecutionStart
	);
	TestTrue(TEXT("A later dodge reserves a fresh window"), NextWindow.IsValid());
	TestTrue(TEXT("The later window applies its tag"), Defense->ActivateDodgeWindow(NextWindow));
	UAbilitySystemComponent* PlayerAbilitySystem = Player->GetAbilitySystemComponent();
	TestEqual(TEXT("Defense owns one invulnerability count"),
		PlayerAbilitySystem->GetTagCount(AshenOathGameplayTags::State_Invulnerable), 1);
	PlayerAbilitySystem->AddLooseGameplayTag(AshenOathGameplayTags::State_Invulnerable);
	TestEqual(TEXT("Independent invulnerability is classified separately"),
		Damage->ApplyDamageAttempt(Attempt), ECombatDamageResult::OtherInvulnerable);
	Defense->EndDodgeWindow(NextWindow);
	TestEqual(TEXT("Cleanup preserves independently owned invulnerability"),
		PlayerAbilitySystem->GetTagCount(AshenOathGameplayTags::State_Invulnerable), 1);
	TestEqual(TEXT("Independent protection remains after the dodge ends"),
		Damage->ApplyDamageAttempt(Attempt), ECombatDamageResult::OtherInvulnerable);
	PlayerAbilitySystem->RemoveLooseGameplayTag(AshenOathGameplayTags::State_Invulnerable);
	TestEqual(TEXT("Damage resumes after all protection ends"),
		Damage->ApplyDamageAttempt(Attempt), ECombatDamageResult::Applied);
	TestEqual(TEXT("Each valid attempt broadcasts its result"), ResolvedResultCount, 9);
	Damage->OnDamageResolved().Remove(DamageResolvedHandle);

	CleanupWorld();
	return true;
}

#endif
