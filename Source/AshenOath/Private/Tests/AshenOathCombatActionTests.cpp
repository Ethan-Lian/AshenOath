#if WITH_DEV_AUTOMATION_TESTS

#include "Characters/AshenOathBossCharacter.h"
#include "Characters/AshenOathPlayerCharacter.h"

#include "AbilitySystem/Ability/AshenOathDodgeAbility.h"
#include "AbilitySystem/AshenOathAttributeSet.h"
#include "AbilitySystem/AshenOathStaminaRecoveryComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Data/AshenOathDodgeActionData.h"
#include "Damage/CombatDamageComponent.h"
#include "Defense/CombatDefenseComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "GameplayTags/AshenOathGameplayTags.h"
#include "Misc/AutomationTest.h"
#include "Targeting/CombatTargetingComponent.h"

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
	UCombatTargetingComponent* Targeting =
		Player->FindComponentByClass<UCombatTargetingComponent>();

	TestNotNull(TEXT("Player owns CombatDefense"), Defense);
	TestNotNull(TEXT("Player owns CombatDamage"), Damage);
	TestNotNull(TEXT("Player owns stamina recovery"), Recovery);
	TestNotNull(TEXT("Player owns CharacterMovement"), Movement);
	TestNotNull(TEXT("Player owns CombatTargeting"), Targeting);

	if (!PlayerAbilitySystem || !Defense || !Damage || !Recovery || !Movement ||
		!Targeting)
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
	auto GetActiveDodgeData = [PlayerAbilitySystem]() -> const UAshenOathDodgeActionData*
	{
		for (const FGameplayAbilitySpec& Spec : PlayerAbilitySystem->GetActivatableAbilities())
		{
			if (Spec.IsActive() && Spec.Ability &&
				Spec.Ability->GetAssetTags().HasTagExact(
					AshenOathGameplayTags::Ability_Action_Dodge))
			{
				return Cast<UAshenOathDodgeActionData>(Spec.SourceObject.Get());
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
				DodgeAbility->TickTasksForTesting(Step);
			}
			Duration -= Step;
		}
	};

	const FGameplayAttribute StaminaAttribute = UAshenOathAttributeSet::GetStaminaAttribute();
	const FGameplayAttribute MaxStaminaAttribute = UAshenOathAttributeSet::GetMaxStaminaAttribute();
	const FGameplayAttribute HealthAttribute = UAshenOathAttributeSet::GetHealthAttribute();
	const float StaminaBeforeDodge = PlayerAbilitySystem->GetNumericAttribute(StaminaAttribute);
	const FVector ForwardDodgeStart = Player->GetActorLocation();
	const EMovementMode MovementModeBeforeDodge = Movement->MovementMode;

	TestTrue(
		TEXT("Configured forward dodge activation is accepted"),
		Player->RequestDodge(FVector2D::ZeroVector)
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

	const UAshenOathDodgeActionData* ForwardDodgeData = GetActiveDodgeData();
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
	DamageAttempt.DamageEffect = UGameplayEffect::StaticClass();
	DamageAttempt.SourceActor = Source;
	DamageAttempt.HitTimeSeconds = World->GetTimeSeconds();

	TestEqual(
		TEXT("A hit inside the dodge window is classified by CombatDefense"),
		Damage->ApplyDamageAttempt(DamageAttempt),
		ECombatDamageResult::DodgeInvulnerable
	);

	PlayerAbilitySystem->AddLooseGameplayTag(AshenOathGameplayTags::State_Invulnerable);
	TestEqual(
		TEXT("Independent invulnerability takes priority over dodge invulnerability"),
		Damage->ApplyDamageAttempt(DamageAttempt),
		ECombatDamageResult::OtherInvulnerable
	);
	PlayerAbilitySystem->RemoveLooseGameplayTag(AshenOathGameplayTags::State_Invulnerable);

	const float StaminaBeforeRejectedRetrigger =
		PlayerAbilitySystem->GetNumericAttribute(StaminaAttribute);
	TestFalse(
		TEXT("An active dodge cannot retrigger"),
		Player->RequestDodge(FVector2D::ZeroVector)
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
	TestTrue(TEXT("Code-driven dodge covers most of its configured distance"),
		ForwardDistance >= ForwardDodgeData->MovementDistance * 0.9f);
	TestTrue(TEXT("Dodge does not exceed its configured distance"),
		ForwardDistance <= ForwardDodgeData->MovementDistance + 1.0f);
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

	const FRotator SideDodgeReferenceRotation(0.0f, 35.0f, 0.0f);
	PlayerController->SetControlRotation(SideDodgeReferenceRotation);
	Player->SetActorRotation(SideDodgeReferenceRotation);
	Source->SetActorLocation(
		Player->GetActorLocation() +
		FRotationMatrix(SideDodgeReferenceRotation).GetUnitAxis(EAxis::X) * 1000.0f
	);
	TestTrue(
		TEXT("Side dodge test acquires its lock target"),
		Targeting->TrySetTarget(Source)
	);
	const float InwardAngle = FMath::Clamp(
		ForwardDodgeData->LockedSideDodgeInwardAngle, 0.0f, 45.0f
	);
	const float SideAngleFromTarget = 90.0f - InwardAngle;
	const FVector SideDodgeToTarget = FRotationMatrix(
		SideDodgeReferenceRotation
	).GetUnitAxis(EAxis::X);
	const FVector ExpectedSideDodgeFacing = SideDodgeToTarget.RotateAngleAxis(
		SideAngleFromTarget, FVector::UpVector
	);
	const FVector SideDodgeStart = Player->GetActorLocation();
	// Camera lag and a previous action's facing must not change the lock-relative path.
	PlayerController->SetControlRotation(FRotator(0.0f, -40.0f, 0.0f));
	Player->SetActorRotation(FRotator(0.0f, -10.0f, 0.0f));
	TestTrue(
		TEXT("Side input activates a forward dodge"),
		Player->RequestDodge(FVector2D(1.0f, 0.0f))
	);
	TestTrue(
		TEXT("Locked right dodge uses the configured inward angle"),
		FVector::DotProduct(
			Player->GetActorForwardVector(),
			ExpectedSideDodgeFacing
		) > 0.99f
	);
	TestTrue(
		TEXT("Active dodge owns movement and facing"),
		PlayerAbilitySystem->HasMatchingGameplayTag(
			AshenOathGameplayTags::State_MovementLocked
		)
	);
	const UAshenOathDodgeActionData* SideDodgeData = GetActiveDodgeData();
	TestNotNull(TEXT("Active side dodge keeps its ActionData source"), SideDodgeData);
	if (!SideDodgeData)
	{
		CleanupWorld();
		return false;
	}
	Source->SetActorLocation(Source->GetActorLocation() + FVector(0.0f, 100.0f, 0.0f));
	PlayerController->SetControlRotation(FRotator(0.0f, 120.0f, 0.0f));
	TickWorldFor(
		SideDodgeData->MovementStartTime +
		SideDodgeData->MovementDuration * 0.95f
	);
	TestTrue(
		TEXT("Side dodge preserves travel-facing until displacement finishes"),
		FVector::DotProduct(
			Player->GetActorForwardVector(), ExpectedSideDodgeFacing
		) > 0.99f
	);
	TestTrue(
		TEXT("Target and camera changes do not bend the frozen inward travel direction"),
		FVector::DotProduct(
			(Player->GetActorLocation() - SideDodgeStart).GetSafeNormal2D(),
			ExpectedSideDodgeFacing
		) > 0.99f
	);
	TickWorldFor(SideDodgeData->MovementDuration * 0.05f + 0.05f);
	TestTrue(
		TEXT("Recovery starts gently instead of immediately snapping to the target"),
		HasActiveDodge() && FVector::DotProduct(
			Player->GetActorForwardVector(), ExpectedSideDodgeFacing
		) > 0.98f
	);
	float MaximumRecoveryYawStep = 0.0f;
	for (int32 Frame = 0; Frame < 22; ++Frame)
	{
		const float PreviousYaw = Player->GetActorRotation().Yaw;
		TickWorldFor(1.0f / 60.0f);
		MaximumRecoveryYawStep = FMath::Max(
			MaximumRecoveryYawStep,
			FMath::Abs(FMath::FindDeltaAngleDegrees(
				PreviousYaw, Player->GetActorRotation().Yaw
			))
		);
	}
	TestTrue(
		TEXT("Side recovery avoids rapid per-frame turns at 60 FPS"),
		MaximumRecoveryYawStep < 8.0f
	);
	const FVector FacingToTarget = (
		Source->GetActorLocation() - Player->GetActorLocation()
	).GetSafeNormal2D();
	TestTrue(
		TEXT("Side dodge recovers toward its lock target before Ability handoff"),
		HasActiveDodge() &&
		FVector::DotProduct(Player->GetActorForwardVector(), FacingToTarget) > 0.99f
	);
	PlayerAbilitySystem->CancelAbilities(&CombatAbilityTags);
	Targeting->ClearTarget();
	TestFalse(
		TEXT("Dodge cancellation releases movement and facing ownership"),
		PlayerAbilitySystem->HasMatchingGameplayTag(
			AshenOathGameplayTags::State_MovementLocked
		)
	);
	PlayerAbilitySystem->SetNumericAttributeBase(
		StaminaAttribute,
		PlayerAbilitySystem->GetNumericAttribute(MaxStaminaAttribute)
	);

	TestTrue(TEXT("Left dodge reacquires its lock target"), Targeting->TrySetTarget(Source));
	const FVector LeftDodgeStart = Player->GetActorLocation();
	const FVector ExpectedLeftDodgeDirection = (
		Source->GetActorLocation() - LeftDodgeStart
	).GetSafeNormal2D().RotateAngleAxis(-SideAngleFromTarget, FVector::UpVector);
	TestTrue(
		TEXT("Locked left input activates a dodge"),
		Player->RequestDodge(FVector2D(-1.0f, 0.0f))
	);
	TickWorldFor(SideDodgeData->MovementStartTime + SideDodgeData->MovementDuration);
	TestTrue(
		TEXT("Locked left dodge uses the configured inward angle on the opposite side"),
		FVector::DotProduct(
			(Player->GetActorLocation() - LeftDodgeStart).GetSafeNormal2D(),
			ExpectedLeftDodgeDirection
		) > 0.99f
	);
	PlayerAbilitySystem->CancelAbilities(&CombatAbilityTags);
	Targeting->ClearTarget();
	PlayerAbilitySystem->SetNumericAttributeBase(
		StaminaAttribute,
		PlayerAbilitySystem->GetNumericAttribute(MaxStaminaAttribute)
	);

	PlayerController->SetControlRotation(FRotator::ZeroRotator);
	const FVector FreeSideDodgeStart = Player->GetActorLocation();
	TestTrue(
		TEXT("Unlocked side input still activates a dodge"),
		Player->RequestDodge(FVector2D(1.0f, 0.0f))
	);
	TickWorldFor(SideDodgeData->MovementStartTime + SideDodgeData->MovementDuration);
	TestTrue(
		TEXT("Unlocked side dodge stays camera-relative without inward bias"),
		FVector::DotProduct(
			(Player->GetActorLocation() - FreeSideDodgeStart).GetSafeNormal2D(),
			FVector::RightVector
		) > 0.99f
	);
	PlayerAbilitySystem->CancelAbilities(&CombatAbilityTags);
	PlayerAbilitySystem->SetNumericAttributeBase(
		StaminaAttribute,
		PlayerAbilitySystem->GetNumericAttribute(MaxStaminaAttribute)
	);

	const FRotator BackwardDodgeFacing(0.0f, -25.0f, 0.0f);
	PlayerController->SetControlRotation(BackwardDodgeFacing);
	Player->SetActorRotation(BackwardDodgeFacing);
	const FVector BackwardDodgeStart = Player->GetActorLocation();
	const FVector ActorForward = Player->GetActorForwardVector();
	Source->SetActorLocation(BackwardDodgeStart + ActorForward * 1000.0f);
	TestTrue(TEXT("Backward dodge acquires its lock target"), Targeting->TrySetTarget(Source));
	TestTrue(
		TEXT("Backward input activates the backward dodge spec"),
		Player->RequestDodge(FVector2D(0.0f, -1.0f))
	);
	TestTrue(
		TEXT("Backward dodge preserves target-facing"),
		FVector::DotProduct(
			Player->GetActorForwardVector(),
			ActorForward
		) > 0.99f
	);
	const UAshenOathDodgeActionData* BackwardDodgeData = GetActiveDodgeData();
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
	Targeting->ClearTarget();

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
	TestTrue(
		TEXT("Dodge toward a wall still activates"),
		Player->RequestDodge(FVector2D::ZeroVector)
	);
	const UAshenOathDodgeActionData* BlockedDodgeData = GetActiveDodgeData();
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

	Recovery->StopRecovery();
	PlayerAbilitySystem->SetNumericAttributeBase(StaminaAttribute, 0.0f);

	FGameplayAbilitySpec* BackwardDodgeSpec = nullptr;
	for (FGameplayAbilitySpec& Spec : PlayerAbilitySystem->GetActivatableAbilities())
	{
		if (Spec.SourceObject.Get() == BackwardDodgeData)
		{
			BackwardDodgeSpec = &Spec;
			break;
		}
	}

	TestNotNull(TEXT("Backward dodge spec remains granted"), BackwardDodgeSpec);
	TestFalse(
		TEXT("Insufficient stamina rejects dodge activation"),
		Player->RequestDodge(FVector2D(0.0f, -1.0f))
	);
	TestFalse(
		TEXT("Rejected request does not restart recovery"),
		Recovery->HasPendingOrActiveRecovery()
	);

	PlayerAbilitySystem->SetNumericAttributeBase(
		StaminaAttribute,
		PlayerAbilitySystem->GetNumericAttribute(MaxStaminaAttribute)
	);
	if (BackwardDodgeSpec)
	{
		TestFalse(
			TEXT("Rejected activation clears its pending movement direction"),
			PlayerAbilitySystem->TryActivateAbility(BackwardDodgeSpec->Handle)
		);
	}

	PlayerAbilitySystem->AddLooseGameplayTag(AshenOathGameplayTags::State_Staggered);
	FGameplayTagContainer ReentrantDefenseTags;
	ReentrantDefenseTags.AddTag(AshenOathGameplayTags::State_Invulnerable);
	ReentrantDefenseTags.AddTag(AshenOathGameplayTags::State_Staggered);
	const FCombatDefenseWindowHandle ReentrantDefenseWindow =
		Defense->BeginDodgeWindow(
			Player,
			ReentrantDefenseTags,
			0.0f,
			1.0f,
			World->GetTimeSeconds()
		);
	const FDelegateHandle InvulnerabilityChangedDelegate =
		PlayerAbilitySystem->RegisterGameplayTagEvent(
			AshenOathGameplayTags::State_Invulnerable,
			EGameplayTagEventType::NewOrRemoved
		).AddLambda(
			[Defense, ReentrantDefenseWindow](const FGameplayTag, const int32 NewCount)
			{
				if (NewCount > 0)
				{
					Defense->EndDodgeWindow(ReentrantDefenseWindow);
				}
			}
		);

	Defense->ActivateDodgeWindow(ReentrantDefenseWindow);
	PlayerAbilitySystem->RegisterGameplayTagEvent(
		AshenOathGameplayTags::State_Invulnerable,
		EGameplayTagEventType::NewOrRemoved
	).Remove(InvulnerabilityChangedDelegate);

	TestFalse(TEXT("Synchronous tag callback releases the Defense window"),
		Defense->OwnsWindow(ReentrantDefenseWindow));
	TestEqual(TEXT("Defense cleanup preserves an independently owned tag count"),
		PlayerAbilitySystem->GetTagCount(AshenOathGameplayTags::State_Staggered),
		1);
	TestEqual(TEXT("Defense cleanup removes its applied invulnerability count"),
		PlayerAbilitySystem->GetTagCount(AshenOathGameplayTags::State_Invulnerable),
		0);
	PlayerAbilitySystem->RemoveLooseGameplayTag(AshenOathGameplayTags::State_Staggered);

	bool bRecoveryWasStoppedDuringApplication = false;
	const FDelegateHandle RecoveryAppliedDelegate =
		PlayerAbilitySystem->OnActiveGameplayEffectAddedDelegateToSelf.AddLambda(
			[Recovery, &bRecoveryWasStoppedDuringApplication](
				UAbilitySystemComponent*,
				const FGameplayEffectSpec&,
				FActiveGameplayEffectHandle)
			{
				bRecoveryWasStoppedDuringApplication = true;
				Recovery->StopRecovery();
			}
		);

	UClass* RecoveryEffectClass = LoadClass<UGameplayEffect>(
		nullptr,
		TEXT("/Game/AshenOath/AbilitySystem/Effects/Player/GE_Player_StaminaRecovery.GE_Player_StaminaRecovery_C")
	);
	TestNotNull(TEXT("Player stamina recovery GameplayEffect loads"), RecoveryEffectClass);
	if (!RecoveryEffectClass)
	{
		PlayerAbilitySystem->OnActiveGameplayEffectAddedDelegateToSelf.Remove(
			RecoveryAppliedDelegate
		);
		CleanupWorld();
		return false;
	}
	Recovery->Configure(RecoveryEffectClass, 0.0f);
	Recovery->NotifyStaminaCostCommitted();
	PlayerAbilitySystem->OnActiveGameplayEffectAddedDelegateToSelf.Remove(
		RecoveryAppliedDelegate
	);

	TestTrue(TEXT("Recovery application can synchronously request cleanup"),
		bRecoveryWasStoppedDuringApplication);
	TestFalse(TEXT("Synchronous cleanup leaves no owned recovery state"),
		Recovery->HasPendingOrActiveRecovery());

	PlayerAbilitySystem->SetNumericAttributeBase(
		StaminaAttribute,
		PlayerAbilitySystem->GetNumericAttribute(MaxStaminaAttribute)
	);
	TestTrue(
		TEXT("A later dodge receives a clean execution"),
		Player->RequestDodge(FVector2D::ZeroVector)
	);
	const UAshenOathDodgeActionData* FinalDodgeData = GetActiveDodgeData();
	TestNotNull(TEXT("Later dodge keeps its ActionData source"), FinalDodgeData);
	if (!FinalDodgeData)
	{
		CleanupWorld();
		return false;
	}
	TickWorldFor(FinalDodgeData->WindowStartTime + 0.02f);
	PlayerAbilitySystem->SetNumericAttributeBase(HealthAttribute, 0.0f);

	TestFalse(TEXT("Death cancels the active dodge"), HasActiveDodge());
	TestFalse(
		TEXT("Death removes Defense-owned invulnerability"),
		PlayerAbilitySystem->HasMatchingGameplayTag(AshenOathGameplayTags::State_Invulnerable)
	);
	TestFalse(TEXT("Death stops stamina recovery"), Recovery->HasPendingOrActiveRecovery());
	TestEqual(
		TEXT("Death disables character movement"),
		Movement->MovementMode.GetValue(),
		MOVE_None
	);

	CleanupWorld();
	return true;
}

#endif
