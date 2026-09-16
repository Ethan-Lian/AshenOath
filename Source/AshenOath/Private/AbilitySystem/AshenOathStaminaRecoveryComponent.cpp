#include "AbilitySystem/AshenOathStaminaRecoveryComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameplayEffect.h"
#include "GameplayTags/AshenOathGameplayTags.h"

UAshenOathStaminaRecoveryComponent::UAshenOathStaminaRecoveryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAshenOathStaminaRecoveryComponent::Configure(
	TSubclassOf<UGameplayEffect> RecoveryEffect,
	const float DelaySeconds)
{
	StopRecovery();
	ConfiguredRecoveryEffect = RecoveryEffect;
	RecoveryDelay = FMath::Max(DelaySeconds, 0.0f);
}

void UAshenOathStaminaRecoveryComponent::NotifyStaminaCostCommitted()
{
	RestartRecovery();
}

void UAshenOathStaminaRecoveryComponent::StopRecovery()
{
	++RecoveryGeneration;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RecoveryTimer);
	}

	const FActiveGameplayEffectHandle EffectToRemove = ActiveRecoveryEffect;
	RecoveryTimer.Invalidate();
	ActiveRecoveryEffect = FActiveGameplayEffectHandle();

	if (EffectToRemove.IsValid())
	{
		if (UAbilitySystemComponent* AbilitySystemComponent =
			ResolveAbilitySystemComponent())
		{
			AbilitySystemComponent->RemoveActiveGameplayEffect(EffectToRemove);
		}
	}
}

bool UAshenOathStaminaRecoveryComponent::HasPendingOrActiveRecovery() const
{
	if (ActiveRecoveryEffect.IsValid())
	{
		return true;
	}

	const UWorld* World = GetWorld();
	return World && World->GetTimerManager().TimerExists(RecoveryTimer);
}

void UAshenOathStaminaRecoveryComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	StopRecovery();
	Super::EndPlay(EndPlayReason);
}

UAbilitySystemComponent* UAshenOathStaminaRecoveryComponent::ResolveAbilitySystemComponent() const
{
	const IAbilitySystemInterface* AbilitySystemOwner =
		Cast<IAbilitySystemInterface>(GetOwner());

	return AbilitySystemOwner
		? AbilitySystemOwner->GetAbilitySystemComponent()
		: nullptr;
}

void UAshenOathStaminaRecoveryComponent::RestartRecovery()
{
	StopRecovery();

	UAbilitySystemComponent* AbilitySystemComponent = ResolveAbilitySystemComponent();
	AActor* Owner = GetOwner();

	if (!ConfiguredRecoveryEffect ||
		!AbilitySystemComponent ||
		!IsValid(Owner) ||
		Owner->IsActorBeingDestroyed() ||
		AbilitySystemComponent->HasMatchingGameplayTag(
			AshenOathGameplayTags::State_Dead))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World || RecoveryDelay <= KINDA_SMALL_NUMBER)
	{
		StartRecovery();
		return;
	}

	World->GetTimerManager().SetTimer(
		RecoveryTimer,
		this,
		&UAshenOathStaminaRecoveryComponent::StartRecovery,
		RecoveryDelay,
		false
	);
}

void UAshenOathStaminaRecoveryComponent::StartRecovery()
{
	RecoveryTimer.Invalidate();
	const uint32 StartingGeneration = RecoveryGeneration;
	UAbilitySystemComponent* AbilitySystemComponent = ResolveAbilitySystemComponent();
	AActor* Owner = GetOwner();
	const UGameplayEffect* RecoveryEffect = ConfiguredRecoveryEffect
		? ConfiguredRecoveryEffect.GetDefaultObject()
		: nullptr;

	if (!AbilitySystemComponent ||
		!IsValid(Owner) ||
		Owner->IsActorBeingDestroyed() ||
		AbilitySystemComponent->HasMatchingGameplayTag(
			AshenOathGameplayTags::State_Dead) ||
		!RecoveryEffect ||
		RecoveryEffect->DurationPolicy == EGameplayEffectDurationType::Instant)
	{
		return;
	}

	FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
	Context.AddSourceObject(Owner);
	const FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(
		ConfiguredRecoveryEffect,
		1.0f,
		Context
	);

	if (Spec.IsValid())
	{
		const FActiveGameplayEffectHandle AppliedEffect =
			AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());

		// Effect application broadcasts synchronously. A callback may stop or
		// restart recovery before the application call returns.
		const bool bStillOwnsThisStart =
			RecoveryGeneration == StartingGeneration &&
			ResolveAbilitySystemComponent() == AbilitySystemComponent &&
			IsValid(Owner) &&
			!Owner->IsActorBeingDestroyed() &&
			!AbilitySystemComponent->HasMatchingGameplayTag(
				AshenOathGameplayTags::State_Dead);

		if (AppliedEffect.WasSuccessfullyApplied() && bStillOwnsThisStart)
		{
			ActiveRecoveryEffect = AppliedEffect;
		}
		else if (AppliedEffect.IsValid())
		{
			AbilitySystemComponent->RemoveActiveGameplayEffect(AppliedEffect);
		}
	}
}
