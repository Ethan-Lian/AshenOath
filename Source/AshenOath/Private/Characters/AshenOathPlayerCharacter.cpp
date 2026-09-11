#include "Characters/AshenOathPlayerCharacter.h"
#include "AbilitySystem/AshenOathAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameplayTags/AshenOathGameplayTags.h"
#include "Actions/CombatActionComponent.h"
#include "Actions/CombatActionData.h"
#include "Actions/CombatMeleeComponent.h"


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

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));

	AbilitySystemComponent->SetIsReplicated(false);

	AttributeSet = CreateDefaultSubobject<UAshenOathAttributeSet>(TEXT("AttributeSet"));
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
}


void AAshenOathPlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
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

void AAshenOathPlayerCharacter::RequestLightAttack()
{
	if (!GetController() || IsActorBeingDestroyed() ||
		AbilitySystemComponent->HasMatchingGameplayTag(AshenOathGameplayTags::State_Dead))
	{
		return;
	}

	FCombatActionHandle ActionHandle;

	CombatActionComponent->TryStartAction(LightAttackAction,ActionHandle);
}
