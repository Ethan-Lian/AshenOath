#if WITH_DEV_AUTOMATION_TESTS

#include "Characters/AshenOathBossCharacter.h"
#include "Characters/AshenOathPlayerCharacter.h"

#include "AbilitySystem/AshenOathAttributeSet.h"
#include "AbilitySystem/AshenOathStaminaRegenerationEffect.h"
#include "AbilitySystemComponent.h"
#include "Actions/CombatActionComponent.h"
#include "Damage/CombatDamageComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTags/AshenOathGameplayTags.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAshenOathDodgeLifecycleTest,
	"AshenOath.Combat.Action.DodgeLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FAshenOathDodgeLifecycleTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	WorldContext.SetCurrentWorld(World);

	FURL URL;
	World->InitializeActorsForPlay(URL);
	World->BeginPlay();

	UClass* PlayerClass = LoadClass<AAshenOathPlayerCharacter>(
		nullptr,
		TEXT("/Game/AshenOath/Characters/BP_AshenOathPlayer.BP_AshenOathPlayer_C")
	);
	AAshenOathPlayerCharacter* Player = PlayerClass
		                                    ? World->SpawnActor<AAshenOathPlayerCharacter>(PlayerClass)
		                                    : nullptr;
	APlayerController* PlayerController = World->SpawnActor<APlayerController>();
	AAshenOathBossCharacter* Source = World->SpawnActor<AAshenOathBossCharacter>();

	TestNotNull(TEXT("Playable player Blueprint loads"), Player);
	TestNotNull(TEXT("Test controller spawns"), PlayerController);
	TestNotNull(TEXT("Damage source spawns"), Source);

	if (!Player || !PlayerController || !Source)
	{
		World->EndPlay(EEndPlayReason::Quit);
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
		return false;
	}

	// The focused transient world does not route post-BeginPlay spawns through
	// the normal game loop, so dispatch the same actor lifecycle explicitly.
	Player->DispatchBeginPlay();
	Source->DispatchBeginPlay();
	PlayerController->Possess(Player);

	UAbilitySystemComponent* PlayerAbilitySystem = Player->GetAbilitySystemComponent();
	UCombatActionComponent* Action = Player->FindComponentByClass<UCombatActionComponent>();
	UCombatDamageComponent* Damage = Player->FindComponentByClass<UCombatDamageComponent>();
	UCharacterMovementComponent* Movement = Player->GetCharacterMovement();

	TestNotNull(TEXT("Player owns CombatAction"), Action);
	TestNotNull(TEXT("Player owns CombatDamage"), Damage);
	TestNotNull(TEXT("Player owns CharacterMovement"), Movement);

	if (!PlayerAbilitySystem || !Action || !Damage || !Movement)
	{
		World->EndPlay(EEndPlayReason::Quit);
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
		return false;
	}

	const float StaminaBeforeDodge = PlayerAbilitySystem->GetNumericAttribute(
		UAshenOathAttributeSet::GetStaminaAttribute()
	);
	const FVector StartLocation = Player->GetActorLocation();
	const EMovementMode MovementModeBeforeDodge = Movement->MovementMode;
	const ECombatActionStartResult StartResult = Player->RequestDodge(FVector2D::ZeroVector);
	const float StaminaAfterCost = PlayerAbilitySystem->GetNumericAttribute(
		UAshenOathAttributeSet::GetStaminaAttribute()
	);

	TestEqual(TEXT("Configured dodge starts"), StartResult, ECombatActionStartResult::Started);
	TestTrue(TEXT("Dodge owns an active action"), Action->IsActionActive());
	TestTrue(TEXT("Successful dodge commits one stamina cost"), StaminaAfterCost < StaminaBeforeDodge);
	TestTrue(TEXT("Successful cost schedules delayed stamina recovery"),
	         Action->HasPendingOrActiveResourceRecovery());
	TestFalse(
		TEXT("Invulnerability does not cover the opening windup"),
		PlayerAbilitySystem->HasMatchingGameplayTag(AshenOathGameplayTags::State_Invulnerable)
	);

	Action->TickComponent(0.12f, ELevelTick::LEVELTICK_All, nullptr);

	TestTrue(
		TEXT("Dodge grants invulnerability inside the configured window"),
		PlayerAbilitySystem->HasMatchingGameplayTag(AshenOathGameplayTags::State_Invulnerable)
	);

	FCombatDamageAttempt DamageAttempt;
	DamageAttempt.DamageEffect = UAshenOathStaminaRegenerationEffect::StaticClass();
	DamageAttempt.SourceActor = Source;
	DamageAttempt.AttackInstanceId = 1;
	DamageAttempt.HitId = 1;
	DamageAttempt.HitTimeSeconds = World->GetTimeSeconds() + 0.12;

	TestEqual(
		TEXT("A hit inside the dodge window is rejected as dodge invulnerability"),
		Damage->ApplyDamageAttempt(DamageAttempt),
		ECombatDamageResult::DodgeInvulnerable
	);

	PlayerAbilitySystem->AddLooseGameplayTag(AshenOathGameplayTags::State_Invulnerable);
	DamageAttempt.HitId = 2;
	TestEqual(
		TEXT("Independent invulnerability takes priority over the dodge window"),
		Damage->ApplyDamageAttempt(DamageAttempt),
		ECombatDamageResult::OtherInvulnerable
	);
	PlayerAbilitySystem->RemoveLooseGameplayTag(AshenOathGameplayTags::State_Invulnerable);

	Action->TickComponent(0.4f, ELevelTick::LEVELTICK_All, nullptr);

	TestFalse(
		TEXT("Invulnerability is removed after its window"),
		PlayerAbilitySystem->HasMatchingGameplayTag(AshenOathGameplayTags::State_Invulnerable)
	);

	const float HorizontalDistance = FVector::Dist2D(StartLocation, Player->GetActorLocation());
	TestTrue(TEXT("Code-driven dodge moves the character"), HorizontalDistance > 300.0f);
	TestTrue(TEXT("Dodge displacement remains within its configured distance"), HorizontalDistance <= 425.0f);

	Action->CancelCurrentAction(0.0f);
	TestFalse(TEXT("Cancellation clears the action"), Action->IsActionActive());
	TestEqual(TEXT("Dodge restores the prior movement mode"), Movement->MovementMode.GetValue(), MovementModeBeforeDodge);

	TestEqual(TEXT("A later dodge gets a new execution"),
	          Player->RequestDodge(FVector2D::ZeroVector), ECombatActionStartResult::Started);
	Action->TickComponent(0.12f, ELevelTick::LEVELTICK_All, nullptr);
	PlayerAbilitySystem->AddLooseGameplayTag(AshenOathGameplayTags::State_Dead);
	TestFalse(TEXT("Death cancels the active dodge"), Action->IsActionActive());
	TestFalse(TEXT("Death removes action-owned invulnerability"),
	          PlayerAbilitySystem->HasMatchingGameplayTag(AshenOathGameplayTags::State_Invulnerable));
	TestFalse(TEXT("Death stops stamina recovery owned by the action component"),
	          Action->HasPendingOrActiveResourceRecovery());
	PlayerAbilitySystem->RemoveLooseGameplayTag(AshenOathGameplayTags::State_Dead);

	World->EndPlay(EEndPlayReason::Quit);
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);

	return true;
}

#endif
