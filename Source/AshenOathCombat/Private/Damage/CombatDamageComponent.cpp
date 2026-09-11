#include "Damage/CombatDamageComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameplayEffect.h"

UCombatDamageComponent::UCombatDamageComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

ECombatDamageResult UCombatDamageComponent::ApplyDamageAttempt(const FCombatDamageAttempt& Attempt)
{
	AActor* TargetActor = GetOwner();
	AActor* SourceActor = Attempt.SourceActor.Get();

	if (!IsValid(TargetActor) || TargetActor->IsActorBeingDestroyed() || !IsValid(SourceActor) ||
	    SourceActor->IsActorBeingDestroyed() || !Attempt.DamageEffect || Attempt.AttackInstanceId <= 0 ||
	    Attempt.HitId <= 0)
	{
		return ECombatDamageResult::Invalid;
	}

	UAbilitySystemComponent* SourceAbilitySystemComponent = ResolveAbilitySystemComponent(SourceActor);
	UAbilitySystemComponent* TargetAbilitySystemComponent = ResolveAbilitySystemComponent(TargetActor);

	if (!SourceAbilitySystemComponent || !TargetAbilitySystemComponent)
	{
		return ECombatDamageResult::Invalid;
	}

	FGameplayEffectContextHandle EffectContext = SourceAbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(SourceActor);

	const FGameplayEffectSpecHandle EffectSpec = 
	SourceAbilitySystemComponent->MakeOutgoingSpec(Attempt.DamageEffect, 1.0f, EffectContext);

	if (!EffectSpec.IsValid())
	{
		return ECombatDamageResult::Invalid;
	}

	const FActiveGameplayEffectHandle AppliedHandle =
	SourceAbilitySystemComponent->ApplyGameplayEffectSpecToTarget(*EffectSpec.Data.Get(), TargetAbilitySystemComponent);

	return AppliedHandle.WasSuccessfullyApplied() ? ECombatDamageResult::Applied : ECombatDamageResult::Invalid;
}

UAbilitySystemComponent* UCombatDamageComponent::ResolveAbilitySystemComponent(AActor* Actor)
{
	// Depend on the public ASC contract rather than concrete player or enemy types.
	IAbilitySystemInterface* AbilitySystemOwner = Cast<IAbilitySystemInterface>(Actor);

	return AbilitySystemOwner ? AbilitySystemOwner->GetAbilitySystemComponent() : nullptr;
}
