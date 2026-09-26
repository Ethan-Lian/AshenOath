#include "Characters/AshenOathPlayerCharacter.h"
#include "AbilitySystem/AshenOathAttributeSet.h"
#include "AbilitySystem/AshenOathStaminaRecoveryComponent.h"
#include "AbilitySystem/Data/AshenOathHeavyAttackData.h"
#include "AbilitySystem/Data/AshenOathComboAttackData.h"
#include "AbilitySystem/Data/AshenOathHealActionData.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameplayTags/AshenOathGameplayTags.h"
#include "AbilitySystem/Data/AshenOathActionData.h"
#include "AbilitySystem/Data/AshenOathDodgeActionData.h"
#include "Actions/CombatMeleeComponent.h"
#include "Damage/CombatDamageComponent.h"
#include "Defense/CombatDefenseComponent.h"
#include "AbilitySystem/Ability/AshenOathDodgeAbility.h"
#include "AbilitySystem/Ability/AshenOathComboAttackAbility.h"
#include "AbilitySystem/Ability/AshenOathHeavyAttackAbility.h"
#include "AbilitySystem/Ability/AshenOathHealAbility.h"
#include "Reaction/CombatHitReactionComponent.h"
#include "Death/CombatDeathComponent.h"
#include "GameplayAbilitySpec.h"
#include "Characters/AshenOathBossCharacter.h"
#include "Game/AshenOathGameMode.h"
#include "Targeting/CombatTargetingComponent.h"


AAshenOathPlayerCharacter::AAshenOathPlayerCharacter()
{
	// Free movement separates body facing from view direction: CharacterMovement
	// turns the body, while the boom reads ControlRotation and the camera follows its socket.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->bUseControllerDesiredRotation = false;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetRootComponent());
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bDoCollisionTest = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	CombatTargetingComponent = CreateDefaultSubobject<UCombatTargetingComponent>(TEXT("CombatTargetingComponent"));
	CombatTargetingComponent->ConfigureTerminalStateTag(
		AshenOathGameplayTags::State_Dead
	);

	CombatMeleeComponent = CreateDefaultSubobject<UCombatMeleeComponent>(TEXT("CombatMeleeComponent"));
	CombatDamageComponent = CreateDefaultSubobject<UCombatDamageComponent>(TEXT("CombatDamageComponent"));
	CombatDefenseComponent = CreateDefaultSubobject<UCombatDefenseComponent>(TEXT("CombatDefenseComponent"));
	StaminaRecoveryComponent = CreateDefaultSubobject<UAshenOathStaminaRecoveryComponent>(
		TEXT("StaminaRecoveryComponent")
	);
	HitReactionComponent = CreateDefaultSubobject<UCombatHitReactionComponent>(
		TEXT("HitReactionComponent")
	);
	HitReactionComponent->ConfigureGameplayTags(
		AshenOathGameplayTags::State_Staggered,
		AshenOathGameplayTags::State_Dead,
		AshenOathGameplayTags::Ability_Action
	);
	DeathComponent = CreateDefaultSubobject<UCombatDeathComponent>(
		TEXT("DeathComponent")
	);
	DeathComponent->ConfigureDeathContract(
		UAshenOathAttributeSet::GetHealthAttribute(),
		AshenOathGameplayTags::State_Dead
	);

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));

	AbilitySystemComponent->SetIsReplicated(false);

	AttributeSet = CreateDefaultSubobject<UAshenOathAttributeSet>(TEXT("AttributeSet"));

	ComboAttackAbilityClass = UAshenOathComboAttackAbility::StaticClass();
	HeavyAttackAbilityClass = UAshenOathHeavyAttackAbility::StaticClass();
	HealAbilityClass = UAshenOathHealAbility::StaticClass();
	DodgeAbilityClass = UAshenOathDodgeAbility::StaticClass();
}

void AAshenOathPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (CombatTargetingComponent)
	{
		LockTargetChangedHandle = CombatTargetingComponent->OnTargetChanged().AddUObject(
			this,
			&AAshenOathPlayerCharacter::HandleLockTargetChanged
		);
	}

	if (DeathComponent)
	{
		DeathStartedHandle = DeathComponent->OnDeathStarted().AddUObject(
			this,
			&AAshenOathPlayerCharacter::HandleDeathStarted
		);
	}

	if (AbilitySystemComponent)
	{
		MovementLockedStateChangedHandle = AbilitySystemComponent->RegisterGameplayTagEvent(
			AshenOathGameplayTags::State_MovementLocked,
			EGameplayTagEventType::NewOrRemoved
		).AddUObject(this, &AAshenOathPlayerCharacter::HandleMovementLockedStateChanged);
		StaggeredStateChangedHandle = AbilitySystemComponent->RegisterGameplayTagEvent(
			AshenOathGameplayTags::State_Staggered,
			EGameplayTagEventType::NewOrRemoved
		).AddUObject(this, &AAshenOathPlayerCharacter::HandleMovementLockedStateChanged);
	}

	if (StaminaRecoveryComponent)
	{
		StaminaRecoveryComponent->Configure(
			StaminaRecoveryEffect,
			StaminaRecoveryDelay
		);
	}

	if (CombatDamageComponent)
	{
		CombatDamageComponent->ConfigureInvulnerabilityTag(AshenOathGameplayTags::State_Invulnerable);
		CombatDamageComponent->ConfigureTerminalStateTag(AshenOathGameplayTags::State_Dead);
	}

	if (bInitialAttributesApplied && DeathComponent)
	{
		DeathComponent->Initialize(AbilitySystemComponent);
	}

	RefreshFacingMode();
}

void AAshenOathPlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	check(AbilitySystemComponent);
	check(AttributeSet);

	// Possession is the point at which the authoritative player Controller is known.
	// Reinitializing ActorInfo also refreshes GAS's cached controller/avatar references.
	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	ApplyInitialAttributes();
	GrantConfiguredAbilities();

	if (bInitialAttributesApplied && DeathComponent)
	{
		DeathComponent->Initialize(AbilitySystemComponent);
	}

	RefreshFacingMode();
}

void AAshenOathPlayerCharacter::UnPossessed()
{
	RequestClearLockOn();
	if (AbilitySystemComponent && HealAbilitySpecHandle.IsValid())
	{
		AbilitySystemComponent->CancelAbilityHandle(HealAbilitySpecHandle);
	}
	Super::UnPossessed();
}

void AAshenOathPlayerCharacter::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateLockedView(DeltaSeconds);
}


void AAshenOathPlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (CombatTargetingComponent && LockTargetChangedHandle.IsValid())
	{
		CombatTargetingComponent->OnTargetChanged().Remove(LockTargetChangedHandle);
		LockTargetChangedHandle.Reset();
	}

	RequestClearLockOn();

	if (DeathComponent && DeathStartedHandle.IsValid())
	{
		DeathComponent->OnDeathStarted().Remove(DeathStartedHandle);
		DeathStartedHandle.Reset();
	}

	if (AbilitySystemComponent && MovementLockedStateChangedHandle.IsValid())
	{
		AbilitySystemComponent->RegisterGameplayTagEvent(
			AshenOathGameplayTags::State_MovementLocked,
			EGameplayTagEventType::NewOrRemoved
		).Remove(MovementLockedStateChangedHandle);
		MovementLockedStateChangedHandle.Reset();
	}

	if (AbilitySystemComponent && StaggeredStateChangedHandle.IsValid())
	{
		AbilitySystemComponent->RegisterGameplayTagEvent(
			AshenOathGameplayTags::State_Staggered,
			EGameplayTagEventType::NewOrRemoved
		).Remove(StaggeredStateChangedHandle);
		StaggeredStateChangedHandle.Reset();
	}

	// Ability cancellation must happen while ActorInfo is still valid so its
	// tasks can stop animation and release GAS-owned blocking state.
	CancelCombatAbilities();

	if (StaminaRecoveryComponent)
	{
		StaminaRecoveryComponent->StopRecovery();
	}

	ComboAttackAbilitySpecHandle = FGameplayAbilitySpecHandle();
	HeavyAttackAbilitySpecHandle = FGameplayAbilitySpecHandle();
	HealAbilitySpecHandle = FGameplayAbilitySpecHandle();
	ForwardDodgeAbilitySpecHandle = FGameplayAbilitySpecHandle();
	BackwardDodgeAbilitySpecHandle = FGameplayAbilitySpecHandle();

	if (AbilitySystemComponent)
	{
		// ActorInfo contains weak references into the world; release them before teardown.
		AbilitySystemComponent->ClearActorInfo();
	}

	Super::EndPlay(EndPlayReason);
}

UAbilitySystemComponent* AAshenOathPlayerCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

const UAshenOathAttributeSet* AAshenOathPlayerCharacter::GetAttributeSet() const
{
	return AttributeSet;
}

void AAshenOathPlayerCharacter::RequestMove(const FVector2D& MovementIntent, float ReferenceYaw)
{
	if (!GetController() || IsActorBeingDestroyed() ||
		AbilitySystemComponent->HasMatchingGameplayTag(AshenOathGameplayTags::State_Dead) ||
		AbilitySystemComponent->HasMatchingGameplayTag(AshenOathGameplayTags::State_Staggered) ||
		AbilitySystemComponent->HasMatchingGameplayTag(AshenOathGameplayTags::State_MovementLocked))
	{
		return;
	}

	if (CombatTargetingComponent && CombatTargetingComponent->HasTarget())
	{
		// apply input in the Character's target-facing
		// forward/right frame, then return so free movement cannot also run.
		AddMovementInput(GetActorForwardVector(), MovementIntent.Y);
		AddMovementInput(GetActorRightVector(), MovementIntent.X);
		return;
	}

	const FRotator YawRotation(0.0f, ReferenceYaw, 0.0f);
	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	// Input uses X=right and Y=forward, so the axis pairing below is intentional.
	AddMovementInput(ForwardDirection, MovementIntent.Y);
	AddMovementInput(RightDirection, MovementIntent.X);
}

void AAshenOathPlayerCharacter::ApplyInitialAttributes()
{
	if (bInitialAttributesApplied || !InitialAttributesEffect)
	{
		return;
	}

	FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();

	EffectContext.AddSourceObject(this);

	const FGameplayEffectSpecHandle EffectSpec =
		AbilitySystemComponent->MakeOutgoingSpec(
			InitialAttributesEffect,
			1.0f,
			EffectContext
		);

	if (!EffectSpec.IsValid())
	{
		return;
	}

	const FActiveGameplayEffectHandle AppliedHandle =
		AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*EffectSpec.Data.Get());

	if (AppliedHandle.WasSuccessfullyApplied())
	{
		bInitialAttributesApplied = true;
	}
}

bool AAshenOathPlayerCharacter::RequestComboAttack()
{
	if (!AbilitySystemComponent || IsActorBeingDestroyed())
	{
		return false;
	}

	FGameplayAbilitySpec* ComboAttackSpec =
		AbilitySystemComponent->FindAbilitySpecFromHandle(ComboAttackAbilitySpecHandle);

	if (ComboAttackSpec && ComboAttackSpec->IsActive())
	{
		UAshenOathComboAttackAbility* ComboAttackAbility =
			Cast<UAshenOathComboAttackAbility>(ComboAttackSpec->GetPrimaryInstance());

		return ComboAttackAbility && ComboAttackAbility->TryQueueComboInput();
	}

	return ComboAttackAbilitySpecHandle.IsValid() &&
		AbilitySystemComponent->TryActivateAbility(ComboAttackAbilitySpecHandle);
}

bool AAshenOathPlayerCharacter::RequestHeavyAttackPressed()
{
	if (!AbilitySystemComponent || IsActorBeingDestroyed() ||
		!HeavyAttackAbilitySpecHandle.IsValid())
	{
		return false;
	}

	FGameplayAbilitySpec* HeavyAttackSpec =
		AbilitySystemComponent->FindAbilitySpecFromHandle(
			HeavyAttackAbilitySpecHandle
		);

	if (!HeavyAttackSpec)
	{
		return false;
	}

	if (HeavyAttackSpec->IsActive())
	{
		return false;
	}

	return AbilitySystemComponent->TryActivateAbility(
		HeavyAttackAbilitySpecHandle
	);
}

bool AAshenOathPlayerCharacter::RequestHeavyAttackReleased()
{
	if (!AbilitySystemComponent || IsActorBeingDestroyed())
	{
		return false;
	}

	FGameplayAbilitySpec* HeavyAttackSpec =
		AbilitySystemComponent->FindAbilitySpecFromHandle(
			HeavyAttackAbilitySpecHandle
		);
	UAshenOathHeavyAttackAbility* HeavyAttackAbility =
		HeavyAttackSpec && HeavyAttackSpec->IsActive()
			? Cast<UAshenOathHeavyAttackAbility>(
				HeavyAttackSpec->GetPrimaryInstance()
			)
			: nullptr;

	return HeavyAttackAbility && HeavyAttackAbility->HandleInputReleased();
}

bool AAshenOathPlayerCharacter::RequestHeal()
{
	return CanStartHeal() && HealAbilitySpecHandle.IsValid() &&
		AbilitySystemComponent->TryActivateAbility(HealAbilitySpecHandle);
}

bool AAshenOathPlayerCharacter::CanStartHeal() const
{
	if (!GetController() || IsActorBeingDestroyed() ||
		!AbilitySystemComponent || RemainingHealUses <= 0 || bHealUseReserved)
	{
		return false;
	}

	const float Health = AbilitySystemComponent->GetNumericAttribute(
		UAshenOathAttributeSet::GetHealthAttribute());
	const float MaxHealth = AbilitySystemComponent->GetNumericAttribute(
		UAshenOathAttributeSet::GetMaxHealthAttribute());
	return Health > 0.0f && Health < MaxHealth;
}

bool AAshenOathPlayerCharacter::TryReserveHealUse()
{
	if (!CanStartHeal())
	{
		return false;
	}

	bHealUseReserved = true;

	return true;
}

void AAshenOathPlayerCharacter::ResolveHealUseReservation(const bool bEffectApplied)
{
	if (!bHealUseReserved)
	{
		return;
	}

	bHealUseReserved = false;

	if (!bEffectApplied)
	{
		return;
	}

	--RemainingHealUses;
	HealUsesChangedEvent.Broadcast(RemainingHealUses);
}

bool AAshenOathPlayerCharacter::RequestDodge(const FVector2D& MovementIntent)
{
	if (!GetController() || IsActorBeingDestroyed() || !AbilitySystemComponent)
	{
		return false;
	}

	FGameplayAbilitySpec* HeavyAttackSpec =
		AbilitySystemComponent->FindAbilitySpecFromHandle(
			HeavyAttackAbilitySpecHandle
		);
	UAshenOathHeavyAttackAbility* HeavyAttackAbility =
		HeavyAttackSpec && HeavyAttackSpec->IsActive()
			? Cast<UAshenOathHeavyAttackAbility>(
				HeavyAttackSpec->GetPrimaryInstance()
			)
			: nullptr;

	// The charge gesture owns Ability.Action and would otherwise block Dodge.
	// Cancel it first; EndAbility invalidates the later release input.
	if (HeavyAttackAbility)
	{
		HeavyAttackAbility->CancelChargeForDodge();
	}

	const FVector2D DodgeIntent = MovementIntent.GetSafeNormal();

	// A rear cone preserves target-facing for the backward asset. Every other
	// direction aligns the forward dodge animation with its displacement.
	const bool bWantsBackwardDodge = DodgeIntent.Y < -0.5f;
	const FGameplayAbilitySpecHandle DodgeAbilityHandle = bWantsBackwardDodge
		? BackwardDodgeAbilitySpecHandle
		: ForwardDodgeAbilitySpecHandle;
	const UAshenOathDodgeActionData* DodgeAction = bWantsBackwardDodge
		? BackwardDodgeAction
		: ForwardDodgeAction;
	const EAshenOathDodgeFacingMode FacingMode = bWantsBackwardDodge
		? EAshenOathDodgeFacingMode::PreserveCurrentFacing
		: EAshenOathDodgeFacingMode::FaceMovementDirection;
	const FVector DodgeDirection = CalculateDodgeDirection(DodgeIntent, DodgeAction);

	FGameplayAbilitySpec* DodgeSpec =
		AbilitySystemComponent->FindAbilitySpecFromHandle(DodgeAbilityHandle);
	UAshenOathDodgeAbility* DodgeAbility = DodgeSpec
		? Cast<UAshenOathDodgeAbility>(DodgeSpec->GetPrimaryInstance())
		: nullptr;

	return DodgeAbility && DodgeAbility->TryActivateWithMovementDirection(
		DodgeDirection,
		FacingMode
	);
}

FVector AAshenOathPlayerCharacter::CalculateDodgeDirection(
	const FVector2D& DodgeIntent,
	const UAshenOathDodgeActionData* DodgeAction) const
{
	FVector DodgeDirection = GetActorForwardVector();

	if (!DodgeIntent.IsNearlyZero())
	{
		const float ReferenceYaw = GetController()
			? GetController()->GetControlRotation().Yaw
			: GetActorRotation().Yaw;
		const FRotator YawRotation(0.0f, ReferenceYaw, 0.0f);
		DodgeDirection =
			FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X) * DodgeIntent.Y +
			FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y) * DodgeIntent.X;

		const AActor* LockTarget = CombatTargetingComponent
			? CombatTargetingComponent->GetTarget()
			: nullptr;
		if (IsValid(LockTarget) && DodgeAction &&
			FMath::IsNearlyZero(DodgeIntent.Y))
		{
			const FVector ToTarget = (
				LockTarget->GetActorLocation() - GetActorLocation()
			).GetSafeNormal2D();
			if (!ToTarget.IsNearlyZero())
			{
				// Use the target bearing, not the lagging camera or travel-facing body.
				const FVector TargetRight = FVector::CrossProduct(FVector::UpVector, ToTarget);
				const float InwardAngle = FMath::DegreesToRadians(
					FMath::Clamp(DodgeAction->LockedSideDodgeInwardAngle, 0.0f, 45.0f)
				);
				DodgeDirection = TargetRight * FMath::Sign(DodgeIntent.X) * FMath::Cos(InwardAngle)
					+ ToTarget * FMath::Sin(InwardAngle);
			}
		}
	}

	return DodgeDirection;
}

bool AAshenOathPlayerCharacter::RequestToggleLockOn()
{
	if (!CombatTargetingComponent || !GetController() || IsActorBeingDestroyed() ||
		!AbilitySystemComponent ||
		AbilitySystemComponent->HasMatchingGameplayTag(AshenOathGameplayTags::State_Dead))
	{
		return false;
	}

	if (CombatTargetingComponent->HasTarget())
	{
		CombatTargetingComponent->ClearTarget();
		return true;
	}

	UWorld* World = GetWorld();
	AAshenOathGameMode* GameMode = World
		? World->GetAuthGameMode<AAshenOathGameMode>()
		: nullptr;

	return GameMode && CombatTargetingComponent->TrySetTarget(GameMode->GetActiveBoss());
}

void AAshenOathPlayerCharacter::RequestClearLockOn()
{
	if (CombatTargetingComponent)
	{
		CombatTargetingComponent->ClearTarget();
	}
}

bool AAshenOathPlayerCharacter::IsLockedOn() const
{
	return CombatTargetingComponent && CombatTargetingComponent->HasTarget();
}

void AAshenOathPlayerCharacter::HandleDeathStarted()
{
	RequestClearLockOn();
	CancelCombatAbilities();

	if (StaminaRecoveryComponent)
	{
		StaminaRecoveryComponent->StopRecovery();
	}

	if (CombatDefenseComponent)
	{
		CombatDefenseComponent->ResetDefenseState();
	}

	if (CombatMeleeComponent)
	{
		CombatMeleeComponent->ResetCombatState();
	}

	if (HitReactionComponent)
	{
		HitReactionComponent->ResetReaction();
	}

	ConsumeMovementInputVector();
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}

	if (UWorld* World = GetWorld())
	{
		if (AAshenOathGameMode* GameMode =
			World->GetAuthGameMode<AAshenOathGameMode>())
		{
			GameMode->ReportPlayerDeath(this);
		}
	}
}

void AAshenOathPlayerCharacter::HandleMovementLockedStateChanged(
	const FGameplayTag,
	const int32 NewCount)
{
	if (NewCount > 0)
	{
		// Input or velocity accumulated earlier in this frame must not turn or slide
		// the character after the attack has claimed its facing direction.
		ConsumeMovementInputVector();

		if (UCharacterMovementComponent* Movement = GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
		}
	}

	RefreshFacingMode();
}

void AAshenOathPlayerCharacter::HandleLockTargetChanged(
	AActor*,
	AActor*)
{
	RefreshFacingMode();
}

void AAshenOathPlayerCharacter::UpdateLockedView(const float DeltaSeconds)
{
	AActor* Target = CombatTargetingComponent
		? CombatTargetingComponent->GetTarget()
		: nullptr;
	AController* CurrentController = GetController();

	if (!IsValid(Target) || !CurrentController)
	{
		return;
	}

	FVector TargetOrigin = Target->GetActorLocation();
	FVector TargetExtent = FVector::ZeroVector;
	Target->GetActorBounds(true, TargetOrigin, TargetExtent);

	// Bias above the bounds center so a large Boss remains readable in frame.
	const FVector FocusPoint = TargetOrigin + FVector(0.0f, 0.0f, TargetExtent.Z * 0.2f);
	const FVector ViewOrigin = FollowCamera
		? FollowCamera->GetComponentLocation()
		: GetPawnViewLocation();
	FRotator DesiredViewRotation = (FocusPoint - ViewOrigin).Rotation();
	DesiredViewRotation.Roll = 0.0f;

	const FRotator SmoothedViewRotation = FMath::RInterpTo(
		CurrentController->GetControlRotation(),
		DesiredViewRotation,
		DeltaSeconds,
		LockOnViewInterpSpeed
	);
	CurrentController->SetControlRotation(SmoothedViewRotation);
}

void AAshenOathPlayerCharacter::RefreshFacingMode()
{
	// arbitrate the three owners.
	// Free movement: orient to movement.
	// Lock-on: use controller desired rotation.
	// State.MovementLocked/State.Staggered/State.Dead: neither system may keep
	// rotating the Character.

	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();

	if (!MovementComponent)
	{
		return;
	}

	const bool bGameplayStateOwnsFacing =
		AbilitySystemComponent &&
		(
			AbilitySystemComponent->HasMatchingGameplayTag(
				AshenOathGameplayTags::State_MovementLocked
			) ||
			AbilitySystemComponent->HasMatchingGameplayTag(
				AshenOathGameplayTags::State_Staggered
			) ||
			AbilitySystemComponent->HasMatchingGameplayTag(
				AshenOathGameplayTags::State_Dead
			)
		);

	const bool bLockOnOwnsFacing =
		!bGameplayStateOwnsFacing && IsLockedOn();

	MovementComponent->bOrientRotationToMovement =
		!bGameplayStateOwnsFacing && !bLockOnOwnsFacing;
	MovementComponent->bUseControllerDesiredRotation =
		bLockOnOwnsFacing;
}

void AAshenOathPlayerCharacter::GrantConfiguredAbilities()
{
	GrantAbilityIfNeeded(
		ComboAttackAbilityClass,
		ComboAttackAction,
		ComboAttackAbilitySpecHandle
	);
	GrantAbilityIfNeeded(
		HeavyAttackAbilityClass,
		HeavyAttackAction,
		HeavyAttackAbilitySpecHandle
	);
	GrantAbilityIfNeeded(
		HealAbilityClass,
		HealAction,
		HealAbilitySpecHandle
	);
	GrantAbilityIfNeeded(
		DodgeAbilityClass,
		ForwardDodgeAction,
		ForwardDodgeAbilitySpecHandle
	);
	GrantAbilityIfNeeded(
		DodgeAbilityClass,
		BackwardDodgeAction,
		BackwardDodgeAbilitySpecHandle
	);
}

void AAshenOathPlayerCharacter::GrantAbilityIfNeeded(
	TSubclassOf<UGameplayAbility> AbilityClass,
	UAshenOathActionData* ActionData,
	FGameplayAbilitySpecHandle& InOutHandle)
{
	if (!AbilitySystemComponent || !AbilityClass || !ActionData)
	{
		return;
	}

	if (InOutHandle.IsValid())
	{
		if (AbilitySystemComponent->FindAbilitySpecFromHandle(InOutHandle))
		{
			return;
		}

		InOutHandle = FGameplayAbilitySpecHandle();
	}

	InOutHandle = AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(
		AbilityClass,
		1,
		INDEX_NONE,
		ActionData
	));
}

void AAshenOathPlayerCharacter::CancelCombatAbilities()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	FGameplayTagContainer AbilityTags;
	AbilityTags.AddTag(AshenOathGameplayTags::Ability_Action);

	AbilitySystemComponent->CancelAbilities(&AbilityTags);
}
