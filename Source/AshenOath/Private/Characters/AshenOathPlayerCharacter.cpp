#include "Characters/AshenOathPlayerCharacter.h"
#include "AbilitySystem/AshenOathAttributeSet.h"
#include "AbilitySystem/AshenOathStaminaRegenerationEffect.h"
#include "AbilitySystem/AshenOathStaminaRecoveryComponent.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameplayTags/AshenOathGameplayTags.h"
#include "Actions/CombatActionData.h"
#include "Actions/CombatMeleeComponent.h"
#include "Damage/CombatDamageComponent.h"
#include "Defense/CombatDefenseComponent.h"
#include "AbilitySystem/Ability/AshenOathDodgeAbility.h"
#include "AbilitySystem/Ability/AshenOathLightAttackAbility.h"
#include "Reaction/CombatHitReactionComponent.h"
#include "Death/CombatDeathComponent.h"
#include "GameplayAbilitySpec.h"
#include "Game/AshenOathGameMode.h"


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

	StaminaRecoveryEffect = UAshenOathStaminaRegenerationEffect::StaticClass();
	LightAttackAbilityClass = UAshenOathLightAttackAbility::StaticClass();
	DodgeAbilityClass = UAshenOathDodgeAbility::StaticClass();
}

void AAshenOathPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

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
}


void AAshenOathPlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
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

	// Ability cancellation must happen while ActorInfo is still valid so its
	// tasks can stop animation and release GAS-owned blocking state.
	CancelCombatAbilities();

	if (StaminaRecoveryComponent)
	{
		StaminaRecoveryComponent->StopRecovery();
	}

	LightAttackAbilitySpecHandle = FGameplayAbilitySpecHandle();
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

bool AAshenOathPlayerCharacter::RequestLightAttack()
{
	if (!AbilitySystemComponent || IsActorBeingDestroyed())
	{
		return false;
	}

	return LightAttackAbilitySpecHandle.IsValid() && AbilitySystemComponent->TryActivateAbility(LightAttackAbilitySpecHandle);
}

bool AAshenOathPlayerCharacter::RequestDodge(const FVector2D& MovementIntent)
{
	if (!GetController() || IsActorBeingDestroyed() || !AbilitySystemComponent)
	{
		return false;
	}

	const FVector2D DodgeIntent = MovementIntent.GetSafeNormal();

	// A rear cone selects the backward asset; neutral and side input reuse the forward dodge.
	const bool bWantsBackwardDodge = DodgeIntent.Y < -0.5f;
	const FGameplayAbilitySpecHandle DodgeAbilityHandle = bWantsBackwardDodge
		? BackwardDodgeAbilitySpecHandle
		: ForwardDodgeAbilitySpecHandle;
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
	}

	FGameplayAbilitySpec* DodgeSpec =
		AbilitySystemComponent->FindAbilitySpecFromHandle(DodgeAbilityHandle);
	UAshenOathDodgeAbility* DodgeAbility = DodgeSpec
		? Cast<UAshenOathDodgeAbility>(DodgeSpec->GetPrimaryInstance())
		: nullptr;

	return DodgeAbility && DodgeAbility->TryActivateWithMovementDirection(DodgeDirection);
}

void AAshenOathPlayerCharacter::HandleDeathStarted()
{
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
	if (NewCount <= 0)
	{
		return;
	}

	// Input or velocity accumulated earlier in this frame must not turn or slide
	// the character after the attack has claimed its facing direction.
	ConsumeMovementInputVector();

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
	}
}

void AAshenOathPlayerCharacter::GrantConfiguredAbilities()
{
	GrantAbilityIfNeeded(
		LightAttackAbilityClass,
		LightAttackAction,
		LightAttackAbilitySpecHandle
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
	UCombatActionData* ActionData,
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
