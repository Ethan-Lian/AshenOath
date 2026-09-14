#include "Damage/CombatDamageComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Defense/CombatDefenseComponent.h"
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

	if (InvulnerabilityTag.IsValid())
	{
		const UCombatDefenseComponent* Defense =
			TargetActor->FindComponentByClass<UCombatDefenseComponent>();
		const bool bDodgeWindowOwnsHitTime = Defense &&
			Defense->IsDodgeWindowActiveAt(
				InvulnerabilityTag,
				Attempt.HitTimeSeconds
			);
		const int32 DefenseOwnedTagCount = Defense &&
			Defense->IsWindowTagApplied(InvulnerabilityTag) ? 1 : 0;

		// Independent invulnerability takes priority over the dodge window so a
		// later perfect-dodge reward cannot be granted while another source is
		// already protecting the target.
		if (TargetAbilitySystemComponent->GetTagCount(InvulnerabilityTag) > DefenseOwnedTagCount)
		{
			return ECombatDamageResult::OtherInvulnerable;
		}

		if (bDodgeWindowOwnsHitTime)
		{
			return ECombatDamageResult::DodgeInvulnerable;
		}
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

void UCombatDamageComponent::ConfigureInvulnerabilityTag(const FGameplayTag& Tag)
{
	InvulnerabilityTag = Tag;
}

UAbilitySystemComponent* UCombatDamageComponent::ResolveAbilitySystemComponent(AActor* Actor)
{
	// Depend on the public ASC contract rather than concrete player or enemy types.
	IAbilitySystemInterface* AbilitySystemOwner = Cast<IAbilitySystemInterface>(Actor);

	return AbilitySystemOwner ? AbilitySystemOwner->GetAbilitySystemComponent() : nullptr;
}
