#include "Characters/AshenOathPlayerCharacter.h"
#include "AbilitySystem/AshenOathAttributeSet.h"
#include "AbilitySystem/AshenOathStaminaRegenerationEffect.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameplayTags/AshenOathGameplayTags.h"
#include "Actions/CombatActionComponent.h"
#include "Actions/CombatActionData.h"
#include "Actions/CombatMeleeComponent.h"
#include "Damage/CombatDamageComponent.h"
#include "AbilitySystem/Ability/AshenOathLightAttackAbility.h"
#include "GameplayAbilitySpec.h"


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

	CombatActionComponent = CreateDefaultSubobject<UCombatActionComponent>(TEXT("CombatActionComponent"));
	CombatMeleeComponent = CreateDefaultSubobject<UCombatMeleeComponent>(TEXT("CombatMeleeComponent"));
	CombatDamageComponent = CreateDefaultSubobject<UCombatDamageComponent>(TEXT("CombatDamageComponent"));

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));

	AbilitySystemComponent->SetIsReplicated(false);

	AttributeSet = CreateDefaultSubobject<UAshenOathAttributeSet>(TEXT("AttributeSet"));

	StaminaRecoveryEffect = UAshenOathStaminaRegenerationEffect::StaticClass();
}

void AAshenOathPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (AbilitySystemComponent)
	{
		DeadStateChangedHandle = AbilitySystemComponent->RegisterGameplayTagEvent(
			AshenOathGameplayTags::State_Dead,
			EGameplayTagEventType::NewOrRemoved
		).AddUObject(this, &AAshenOathPlayerCharacter::HandleDeadStateChanged);
	}

	if (CombatActionComponent)
	{
		CombatActionComponent->ConfigureResourceRecovery(StaminaRecoveryEffect, StaminaRecoveryDelay);
	}

	if (CombatDamageComponent)
	{
		CombatDamageComponent->ConfigureInvulnerabilityTag(AshenOathGameplayTags::State_Invulnerable);
	}
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
}


void AAshenOathPlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AbilitySystemComponent && DeadStateChangedHandle.IsValid())
	{
		AbilitySystemComponent->RegisterGameplayTagEvent(
			AshenOathGameplayTags::State_Dead,
			EGameplayTagEventType::NewOrRemoved
		).Remove(DeadStateChangedHandle);
		DeadStateChangedHandle.Reset();
	}

	// Ability cancellation must happen while ActorInfo is still valid so its
	// tasks can stop animation and release GAS-owned blocking state.
	CancelCombatAbilities();

	if (CombatActionComponent)
	{
		CombatActionComponent->CancelCurrentAction(0.0f);
		CombatActionComponent->StopResourceRecovery();
	}

	LightAttackAbilitySpecHandle = FGameplayAbilitySpecHandle();

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

void AAshenOathPlayerCharacter::RequestMove(const FVector2D& MovementIntent,float ReferenceYaw)
{
	if (!GetController() || IsActorBeingDestroyed() ||
		AbilitySystemComponent->HasMatchingGameplayTag(AshenOathGameplayTags::State_Dead)) return;

	// FRotator uses (Pitch, Yaw, Roll). Keeping only yaw gives a horizontal reference frame.
	// GetUnitAxis returns its rotated local axes in world space: X is forward, Y is right.
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

	// Record which character created this effect. Calculations can read this source later.
	EffectContext.AddSourceObject(this);

	// The GameplayEffect class is only a template. MakeOutgoingSpec creates the
	// runtime effect data GAS can apply, including its level and context.
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

bool AAshenOathPlayerCharacter::RequestLightAttack()
{
	if (!AbilitySystemComponent || IsActorBeingDestroyed())
	{
		return false;
	}

	// Do not let the new GAS path interrupt a legacy dodge.
	if (!CombatActionComponent ||
		CombatActionComponent->IsActionActive())
	{
		return false;
	}

	if (LightAttackAbilityClass)
	{
		return LightAttackAbilitySpecHandle.IsValid() && AbilitySystemComponent->TryActivateAbility(LightAttackAbilitySpecHandle);
	}

	return TryStartCombatAction(LightAttackAction) == ECombatActionStartResult::Started;
}

ECombatActionStartResult AAshenOathPlayerCharacter::RequestDodge(const FVector2D& MovementIntent)
{
	// A normalized threshold gives the backward action a clear rear cone while
	// small sideways stick noise continues to use the reusable forward flip.
	const FVector2D DodgeIntent = MovementIntent.GetSafeNormal();
	const bool bWantsBackwardDodge = DodgeIntent.Y < -0.5f;
	const UCombatActionData* DodgeAction = bWantsBackwardDodge
		                                      ? BackwardDodgeAction.Get()
		                                      : ForwardDodgeAction.Get();
	FVector DodgeDirection = GetActorForwardVector();

	if (!DodgeIntent.IsNearlyZero())
	{
		const float ReferenceYaw = GetController() ? GetController()->GetControlRotation().Yaw : GetActorRotation().Yaw;
		const FRotator YawRotation(0.0f, ReferenceYaw, 0.0f);
		DodgeDirection =
			FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X) * DodgeIntent.Y +
			FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y) * DodgeIntent.X;
	}

	return TryStartCombatAction(DodgeAction, DodgeDirection);
}

ECombatActionStartResult AAshenOathPlayerCharacter::TryStartCombatAction(const UCombatActionData* ActionData,
	                                                                      const FVector& MovementDirection)
{
	if (!GetController() || IsActorBeingDestroyed() || !CombatActionComponent || !AbilitySystemComponent)
	{
		return ECombatActionStartResult::RejectedInvalidOwner;
	}

	if (AbilitySystemComponent->HasMatchingGameplayTag(AshenOathGameplayTags::State_Dead))
	{
		return ECombatActionStartResult::RejectedBlockedByState;
	}

	// Do not let a legacy dodge start while a GAS combat ability is active.
	if (IsCombatAbilityActive())
	{
		return ECombatActionStartResult::RejectedAlreadyActive;
	}

	FCombatActionHandle ActionHandle;
	return CombatActionComponent->TryStartAction(ActionData, MovementDirection, ActionHandle);
}

void AAshenOathPlayerCharacter::HandleDeadStateChanged(const FGameplayTag, const int32 NewCount)
{
	if (NewCount <= 0)
	{
		return;
	}

	CancelCombatAbilities();

	// Dodge and stamina recovery still belong to the legacy component.
	if (CombatActionComponent)
	{
		CombatActionComponent->CancelCurrentAction(0.0f);
		CombatActionComponent->StopResourceRecovery();
	}
}

void AAshenOathPlayerCharacter::GrantConfiguredAbilities()
{
	if (!AbilitySystemComponent ||
		!LightAttackAbilityClass ||
		!LightAttackAction)
	{
		return;
	}

	if (LightAttackAbilitySpecHandle.IsValid())
	{
		if (AbilitySystemComponent->FindAbilitySpecFromHandle(LightAttackAbilitySpecHandle))
		{
			// Repossession refreshes ActorInfo but must not grant a duplicate.
			return;
		}

		// The cached handle became stale because somebody removed its Spec.
		LightAttackAbilitySpecHandle = FGameplayAbilitySpecHandle();
	}

	const FGameplayAbilitySpec LightAttackSpec(
		LightAttackAbilityClass,
		1,
		INDEX_NONE,
		LightAttackAction.Get()
	);

	LightAttackAbilitySpecHandle = AbilitySystemComponent->GiveAbility(LightAttackSpec);
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

bool AAshenOathPlayerCharacter::IsCombatAbilityActive() const
{
	if (!AbilitySystemComponent)
	{
		return false;
	}

	for (const FGameplayAbilitySpec& Spec :
		AbilitySystemComponent->GetActivatableAbilities())
	{
		if (Spec.IsActive() &&
			Spec.Ability &&
			Spec.Ability->GetAssetTags().HasTag(
				AshenOathGameplayTags::Ability_Action
			))
		{
			return true;
		}
	}

	return false;
}
