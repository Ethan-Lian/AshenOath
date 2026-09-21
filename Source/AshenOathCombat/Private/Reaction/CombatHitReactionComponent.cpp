#include "Reaction/CombatHitReactionComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Damage/CombatDamageComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

UCombatHitReactionComponent::UCombatHitReactionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCombatHitReactionComponent::ConfigureGameplayTags(
	const FGameplayTag& ReactionTag,
	const FGameplayTag& InTerminalStateTag,
	const FGameplayTag& InInterruptibleAbilityTag)
{
	ReactionStateTag = ReactionTag;
	TerminalStateTag = InTerminalStateTag;
	InterruptibleAbilityTag = InInterruptibleAbilityTag;
}

void UCombatHitReactionComponent::BeginPlay()
{
	Super::BeginPlay();

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	IAbilitySystemInterface* AbilitySystemOwner =
		Cast<IAbilitySystemInterface>(GetOwner());
	UCombatDamageComponent* DamageComponent = GetOwner()
		? GetOwner()->FindComponentByClass<UCombatDamageComponent>()
		: nullptr;

	if (!IsValid(Character) ||
		!AbilitySystemOwner ||
		!IsValid(DamageComponent) ||
		!ReactionStateTag.IsValid() ||
		!TerminalStateTag.IsValid() ||
		!InterruptibleAbilityTag.IsValid())
	{
		return;
	}

	UAbilitySystemComponent* AbilitySystem =
		AbilitySystemOwner->GetAbilitySystemComponent();
	if (!IsValid(AbilitySystem))
	{
		return;
	}

	CharacterOwner = Character;
	AbilitySystemComponent = AbilitySystem;
	BoundDamageComponent = DamageComponent;
	DamageResolvedDelegateHandle = DamageComponent->OnDamageResolved().AddUObject(
		this,
		&UCombatHitReactionComponent::HandleDamageResolved
	);
}

void UCombatHitReactionComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (UCombatDamageComponent* DamageComponent = BoundDamageComponent.Get())
	{
		if (DamageResolvedDelegateHandle.IsValid())
		{
			DamageComponent->OnDamageResolved().Remove(
				DamageResolvedDelegateHandle
			);
		}
	}

	DamageResolvedDelegateHandle.Reset();
	BoundDamageComponent.Reset();
	EndHitReaction();
	AbilitySystemComponent.Reset();
	CharacterOwner.Reset();

	Super::EndPlay(EndPlayReason);
}

void UCombatHitReactionComponent::ResetReaction()
{
	if (ACharacter* Character = CharacterOwner.Get())
	{
		if (IsValid(HitReactionMontage))
		{
			Character->StopAnimMontage(HitReactionMontage);
		}
	}

	EndHitReaction();
}

void UCombatHitReactionComponent::HandleDamageResolved(
	const FCombatDamageAttempt&,
	const ECombatDamageResult Result)
{
	if (Result != ECombatDamageResult::Applied ||
		!IsValid(HitReactionMontage) ||
		StaggerDuration <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	ACharacter* Character = CharacterOwner.Get();
	UAbilitySystemComponent* AbilitySystem = AbilitySystemComponent.Get();

	if (!IsValid(Character) ||
		Character->IsActorBeingDestroyed() ||
		!IsValid(AbilitySystem) ||
		AbilitySystem->HasMatchingGameplayTag(TerminalStateTag))
	{
		return;
	}

	if (!bOwnsReactionTag)
	{
		// Establish ownership before the synchronous GameplayTag callback.
		bOwnsReactionTag = true;
		AbilitySystem->AddLooseGameplayTag(ReactionStateTag);

		if (!bOwnsReactionTag)
		{
			return;
		}
	}

	FGameplayTagContainer InterruptibleAbilityTags;
	InterruptibleAbilityTags.AddTag(InterruptibleAbilityTag);
	AbilitySystem->CancelAbilities(&InterruptibleAbilityTags);

	if (!bOwnsReactionTag ||
		CharacterOwner.Get() != Character ||
		AbilitySystemComponent.Get() != AbilitySystem)
	{
		return;
	}

	if (UCharacterMovementComponent* Movement =
		Character->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
	}

	if (Character->PlayAnimMontage(HitReactionMontage) <= 0.0f)
	{
		EndHitReaction();
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		EndHitReaction();
		return;
	}

	World->GetTimerManager().SetTimer(
		StaggerTimerHandle,
		this,
		&UCombatHitReactionComponent::EndHitReaction,
		StaggerDuration,
		false
	);
}

void UCombatHitReactionComponent::EndHitReaction()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StaggerTimerHandle);
	}

	if (!bOwnsReactionTag)
	{
		return;
	}

	// Revoke ownership before removing the loose tag because tag callbacks are
	// synchronous and may re-enter cleanup.
	bOwnsReactionTag = false;
	if (UAbilitySystemComponent* AbilitySystem = AbilitySystemComponent.Get())
	{
		AbilitySystem->RemoveLooseGameplayTag(ReactionStateTag);
	}
}
