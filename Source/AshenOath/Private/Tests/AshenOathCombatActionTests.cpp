#if WITH_DEV_AUTOMATION_TESTS

#include "Characters/AshenOathBossCharacter.h"
#include "Characters/AshenOathPlayerCharacter.h"

#include "AbilitySystem/Ability/AshenOathDodgeAbility.h"
#include "AbilitySystem/AshenOathAttributeSet.h"
#include "AbilitySystem/AshenOathStaminaRecoveryComponent.h"
#include "AbilitySystem/AshenOathStaminaRegenerationEffect.h"
#include "AbilitySystemComponent.h"
#include "Actions/CombatActionData.h"
#include "Damage/CombatDamageComponent.h"
#include "Defense/CombatDefenseComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameplayAbilitySpec.h"
#include "GameplayTags/AshenOathGameplayTags.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAshenOathDodgeAbilityLifecycleTest,
	"AshenOath.Combat.Ability.DodgeLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FAshenOathDodgeAbilityLifecycleTest::RunTest(const FString& Parameters)
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
		CleanupWorld();
		return false;
	}

	Player->DispatchBeginPlay();
	Source->DispatchBeginPlay();
	PlayerController->Possess(Player);

	UAbilitySystemComponent* PlayerAbilitySystem = Player->GetAbilitySystemComponent();
	UCombatDefenseComponent* Defense =
		Player->FindComponentByClass<UCombatDefenseComponent>();
	UCombatDamageComponent* Damage =
		Player->FindComponentByClass<UCombatDamageComponent>();
	UAshenOathStaminaRecoveryComponent* Recovery =
		Player->FindComponentByClass<UAshenOathStaminaRecoveryComponent>();
	UCharacterMovementComponent* Movement = Player->GetCharacterMovement();

	TestNotNull(TEXT("Player owns CombatDefense"), Defense);
	TestNotNull(TEXT("Player owns CombatDamage"), Damage);
	TestNotNull(TEXT("Player owns stamina recovery"), Recovery);
	TestNotNull(TEXT("Player owns CharacterMovement"), Movement);

	if (!PlayerAbilitySystem || !Defense || !Damage || !Recovery || !Movement)
	{
		CleanupWorld();
		return false;
	}

	int32 DodgeSpecCount = 0;
	for (const FGameplayAbilitySpec& Spec : PlayerAbilitySystem->GetActivatableAbilities())
	{
		if (Spec.Ability && Spec.Ability->GetAssetTags().HasTagExact(
			AshenOathGameplayTags::Ability_Action_Dodge))
		{
			++DodgeSpecCount;
		}
	}
	TestEqual(TEXT("Possession grants forward and backward dodge specs"), DodgeSpecCount, 2);

	auto HasActiveDodge = [PlayerAbilitySystem]()
	{
		for (const FGameplayAbilitySpec& Spec : PlayerAbilitySystem->GetActivatableAbilities())
		{
			if (Spec.IsActive() && Spec.Ability &&
				Spec.Ability->GetAssetTags().HasTagExact(
					AshenOathGameplayTags::Ability_Action_Dodge))
			{
				return true;
			}
		}
		return false;
	};
	auto GetActiveDodgeData = [PlayerAbilitySystem]() -> const UCombatActionData*
	{
		for (const FGameplayAbilitySpec& Spec : PlayerAbilitySystem->GetActivatableAbilities())
		{
			if (Spec.IsActive() && Spec.Ability &&
				Spec.Ability->GetAssetTags().HasTagExact(
					AshenOathGameplayTags::Ability_Action_Dodge))
			{
				return Cast<UCombatActionData>(Spec.SourceObject.Get());
			}
		}
		return nullptr;
	};
	auto GetActiveDodgeAbility = [PlayerAbilitySystem]() -> UAshenOathDodgeAbility*
	{
		for (FGameplayAbilitySpec& Spec : PlayerAbilitySystem->GetActivatableAbilities())
		{
			if (Spec.IsActive() && Spec.Ability &&
				Spec.Ability->GetAssetTags().HasTagExact(
					AshenOathGameplayTags::Ability_Action_Dodge))
			{
				return Cast<UAshenOathDodgeAbility>(Spec.GetPrimaryInstance());
			}
		}
		return nullptr;
	};
	auto TickWorldFor = [World, Defense, GetActiveDodgeAbility](float Duration)
	{
		while (Duration > KINDA_SMALL_NUMBER)
		{
			const float Step = FMath::Min(Duration, 1.0f / 60.0f);
			World->Tick(ELevelTick::LEVELTICK_All, Step);
			// Focused transient worlds do not automatically register every newly
			// enabled component tick, so drive the same public tick used in play.
			Defense->TickComponent(Step, ELevelTick::LEVELTICK_All, nullptr);
			// GameplayTasks have the same one-engine-frame limitation. This is
			// idempotent when the normal task tick already ran because movement uses
			// absolute world time.
			if (UAshenOathDodgeAbility* DodgeAbility = GetActiveDodgeAbility())
			{
				DodgeAbility->TickMovementTaskForTesting(Step);
			}
			Duration -= Step;
		}
	};

	const FGameplayAttribute StaminaAttribute = UAshenOathAttributeSet::GetStaminaAttribute();
	const FGameplayAttribute MaxStaminaAttribute = UAshenOathAttributeSet::GetMaxStaminaAttribute();
	const float StaminaBeforeDodge = PlayerAbilitySystem->GetNumericAttribute(StaminaAttribute);
	const FVector ForwardDodgeStart = Player->GetActorLocation();
	const EMovementMode MovementModeBeforeDodge = Movement->MovementMode;

	TestEqual(
		TEXT("Configured forward dodge activation is accepted"),
		Player->RequestDodge(FVector2D::ZeroVector),
		ECombatActionStartResult::Started
	);
	TestTrue(TEXT("Dodge ability remains active during its Montage"), HasActiveDodge());
	TestTrue(
		TEXT("Successful dodge commits one stamina cost"),
		PlayerAbilitySystem->GetNumericAttribute(StaminaAttribute) < StaminaBeforeDodge
	);
	TestTrue(
		TEXT("Successful cost schedules delayed stamina recovery"),
		Recovery->HasPendingOrActiveRecovery()
	);
	TestFalse(
		TEXT("Invulnerability does not cover the opening windup"),
		PlayerAbilitySystem->HasMatchingGameplayTag(AshenOathGameplayTags::State_Invulnerable)
	);

	const UCombatActionData* ForwardDodgeData = GetActiveDodgeData();
	TestNotNull(TEXT("Active forward dodge keeps its ActionData source"), ForwardDodgeData);
	if (!ForwardDodgeData)
	{
		CleanupWorld();
		return false;
	}

	const float TimeToInsideForwardWindow = ForwardDodgeData->WindowStartTime + 0.02f;
	TickWorldFor(TimeToInsideForwardWindow);

	TestTrue(
		TEXT("Dodge grants invulnerability inside the configured window"),
		PlayerAbilitySystem->HasMatchingGameplayTag(AshenOathGameplayTags::State_Invulnerable)
	);

	FCombatDamageAttempt DamageAttempt;
	DamageAttempt.DamageEffect = UAshenOathStaminaRegenerationEffect::StaticClass();
	DamageAttempt.SourceActor = Source;
	DamageAttempt.AttackInstanceId = 1;
	DamageAttempt.HitId = 1;
	DamageAttempt.HitTimeSeconds = World->GetTimeSeconds();

	TestEqual(
		TEXT("A hit inside the dodge window is classified by CombatDefense"),
		Damage->ApplyDamageAttempt(DamageAttempt),
		ECombatDamageResult::DodgeInvulnerable
	);

	PlayerAbilitySystem->AddLooseGameplayTag(AshenOathGameplayTags::State_Invulnerable);
	DamageAttempt.HitId = 2;
	TestEqual(
		TEXT("Independent invulnerability takes priority over dodge invulnerability"),
		Damage->ApplyDamageAttempt(DamageAttempt),
		ECombatDamageResult::OtherInvulnerable
	);
	PlayerAbilitySystem->RemoveLooseGameplayTag(AshenOathGameplayTags::State_Invulnerable);

	const float StaminaBeforeRejectedRetrigger =
		PlayerAbilitySystem->GetNumericAttribute(StaminaAttribute);
	TestEqual(
		TEXT("An active dodge cannot retrigger"),
		Player->RequestDodge(FVector2D::ZeroVector),
		ECombatActionStartResult::RejectedAlreadyActive
	);
	TestEqual(
		TEXT("Rejected dodge does not pay another cost"),
		PlayerAbilitySystem->GetNumericAttribute(StaminaAttribute),
		StaminaBeforeRejectedRetrigger
	);
	TestTrue(
		TEXT("Rejected dodge does not clear pending recovery"),
		Recovery->HasPendingOrActiveRecovery()
	);

	const float ForwardRuntimeEnd = FMath::Max(
		ForwardDodgeData->MovementStartTime + ForwardDodgeData->MovementDuration,
		ForwardDodgeData->WindowStartTime + ForwardDodgeData->WindowDuration
	) + 0.05f;
	TickWorldFor(FMath::Max(ForwardRuntimeEnd - TimeToInsideForwardWindow, 0.0f));

	TestFalse(
		TEXT("Invulnerability is removed after its configured window"),
		PlayerAbilitySystem->HasMatchingGameplayTag(AshenOathGameplayTags::State_Invulnerable)
	);
	const float ForwardDistance = FVector::Dist2D(ForwardDodgeStart, Player->GetActorLocation());
	TestTrue(TEXT("Code-driven dodge moves the character"), ForwardDistance > 300.0f);
	TestTrue(TEXT("Dodge stays within its configured distance"), ForwardDistance <= 425.0f);
	TestEqual(
		TEXT("Movement task restores the prior movement mode"),
		Movement->MovementMode.GetValue(),
		MovementModeBeforeDodge
	);

	FGameplayTagContainer CombatAbilityTags;
	CombatAbilityTags.AddTag(AshenOathGameplayTags::Ability_Action);
	PlayerAbilitySystem->CancelAbilities(&CombatAbilityTags);
	PlayerAbilitySystem->SetNumericAttributeBase(
		StaminaAttribute,
		PlayerAbilitySystem->GetNumericAttribute(MaxStaminaAttribute)
	);

	const FVector BackwardDodgeStart = Player->GetActorLocation();
	const FVector ActorForward = Player->GetActorForwardVector();
	TestEqual(
		TEXT("Backward input activates the backward dodge spec"),
		Player->RequestDodge(FVector2D(0.0f, -1.0f)),
		ECombatActionStartResult::Started
	);
	const UCombatActionData* BackwardDodgeData = GetActiveDodgeData();
	TestNotNull(TEXT("Active backward dodge keeps its ActionData source"), BackwardDodgeData);
	if (!BackwardDodgeData)
	{
		CleanupWorld();
		return false;
	}
	const float TimeToBackwardMovementAndWindow = FMath::Max(
		BackwardDodgeData->WindowStartTime + 0.02f,
		BackwardDodgeData->MovementStartTime + 0.02f
	);
	TickWorldFor(TimeToBackwardMovementAndWindow);
	PlayerAbilitySystem->AddLooseGameplayTag(AshenOathGameplayTags::State_Invulnerable);
	PlayerAbilitySystem->CancelAbilities(&CombatAbilityTags);

	TestFalse(TEXT("Cancellation ends the dodge ability"), HasActiveDodge());
	TestFalse(
		TEXT("Cancellation releases the Defense window"),
		Defense->IsDodgeWindowActiveAt(
			AshenOathGameplayTags::State_Invulnerable,
			World->GetTimeSeconds()
		)
	);
	TestEqual(
		TEXT("Cancellation removes only Defense's invulnerability count"),
		PlayerAbilitySystem->GetTagCount(AshenOathGameplayTags::State_Invulnerable),
		1
	);
	PlayerAbilitySystem->RemoveLooseGameplayTag(AshenOathGameplayTags::State_Invulnerable);
	TestEqual(
		TEXT("Cancellation restores movement mode immediately"),
		Movement->MovementMode.GetValue(),
		MovementModeBeforeDodge
	);
	TestTrue(
		TEXT("Backward dodge moved opposite the character forward vector"),
		FVector::DotProduct(Player->GetActorLocation() - BackwardDodgeStart, ActorForward) < 0.0f
	);

	AActor* Wall = World->SpawnActor<AActor>();
	UBoxComponent* WallCollision = Wall ? NewObject<UBoxComponent>(Wall) : nullptr;
	TestNotNull(TEXT("Blocking wall actor spawns"), Wall);
	TestNotNull(TEXT("Blocking wall collision is created"), WallCollision);
	if (!Wall || !WallCollision)
	{
		CleanupWorld();
		return false;
	}
	Wall->SetRootComponent(WallCollision);
	WallCollision->SetBoxExtent(FVector(10.0f, 120.0f, 120.0f));
	WallCollision->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	WallCollision->RegisterComponent();
	const FVector WallTestDirection = Player->GetActorForwardVector();
	Wall->SetActorLocation(Player->GetActorLocation() + WallTestDirection * 120.0f);

	PlayerAbilitySystem->SetNumericAttributeBase(
		StaminaAttribute,
		PlayerAbilitySystem->GetNumericAttribute(MaxStaminaAttribute)
	);
	const FVector BlockedDodgeStart = Player->GetActorLocation();
	TestEqual(
		TEXT("Dodge toward a wall still activates"),
		Player->RequestDodge(FVector2D::ZeroVector),
		ECombatActionStartResult::Started
	);
	const UCombatActionData* BlockedDodgeData = GetActiveDodgeData();
	TestNotNull(TEXT("Blocked dodge keeps its ActionData source"), BlockedDodgeData);
	if (!BlockedDodgeData)
	{
		CleanupWorld();
		return false;
	}
	TickWorldFor(BlockedDodgeData->MovementStartTime +
		BlockedDodgeData->MovementDuration + 0.05f);
	const float BlockedDistance = FVector::Dist2D(
		BlockedDodgeStart,
		Player->GetActorLocation()
	);
	TestTrue(
		TEXT("Collision truncates dodge displacement before configured distance"),
		BlockedDistance < BlockedDodgeData->MovementDistance * 0.75f
	);
	TestEqual(
		TEXT("Blocked movement still restores the prior movement mode"),
		Movement->MovementMode.GetValue(),
		MovementModeBeforeDodge
	);
	PlayerAbilitySystem->CancelAbilities(&CombatAbilityTags);
	Wall->Destroy();

	PlayerAbilitySystem->SetNumericAttributeBase(
		StaminaAttribute,
		PlayerAbilitySystem->GetNumericAttribute(MaxStaminaAttribute)
	);
	TestEqual(
		TEXT("A later dodge receives a clean execution"),
		Player->RequestDodge(FVector2D::ZeroVector),
		ECombatActionStartResult::Started
	);
	const UCombatActionData* FinalDodgeData = GetActiveDodgeData();
	TestNotNull(TEXT("Later dodge keeps its ActionData source"), FinalDodgeData);
	if (!FinalDodgeData)
	{
		CleanupWorld();
		return false;
	}
	TickWorldFor(FinalDodgeData->WindowStartTime + 0.02f);
	PlayerAbilitySystem->AddLooseGameplayTag(AshenOathGameplayTags::State_Dead);

	TestFalse(TEXT("Death cancels the active dodge"), HasActiveDodge());
	TestFalse(
		TEXT("Death removes Defense-owned invulnerability"),
		PlayerAbilitySystem->HasMatchingGameplayTag(AshenOathGameplayTags::State_Invulnerable)
	);
	TestFalse(TEXT("Death stops stamina recovery"), Recovery->HasPendingOrActiveRecovery());
	TestEqual(
		TEXT("Death cleanup restores movement mode"),
		Movement->MovementMode.GetValue(),
		MovementModeBeforeDodge
	);

	PlayerAbilitySystem->RemoveLooseGameplayTag(AshenOathGameplayTags::State_Dead);
	PlayerAbilitySystem->SetNumericAttributeBase(StaminaAttribute, 0.0f);
	TestEqual(
		TEXT("Insufficient stamina rejects dodge activation"),
		Player->RequestDodge(FVector2D::ZeroVector),
		ECombatActionStartResult::RejectedInsufficientResources
	);
	TestFalse(
		TEXT("Rejected request does not restart recovery after death"),
		Recovery->HasPendingOrActiveRecovery()
	);

	CleanupWorld();
	return true;
}

#endif
