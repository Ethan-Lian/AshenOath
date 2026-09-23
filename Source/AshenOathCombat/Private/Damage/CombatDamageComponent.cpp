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
	    SourceActor->IsActorBeingDestroyed() || !Attempt.DamageEffect)
	{
		return ECombatDamageResult::Invalid;
	}

	UAbilitySystemComponent* SourceAbilitySystemComponent = ResolveAbilitySystemComponent(SourceActor);
	UAbilitySystemComponent* TargetAbilitySystemComponent = ResolveAbilitySystemComponent(TargetActor);

	if (!SourceAbilitySystemComponent || !TargetAbilitySystemComponent)
	{
		return ECombatDamageResult::Invalid;
	}

	// A terminal actor neither deals nor receives damage, so both ASCs are checked.
	if (TerminalStateTag.IsValid() &&
		(SourceAbilitySystemComponent->HasMatchingGameplayTag(TerminalStateTag) ||
		 TargetAbilitySystemComponent->HasMatchingGameplayTag(TerminalStateTag)))
	{
		return ECombatDamageResult::Invalid;
	}

	ECombatDamageResult InvulnerabilityResult = ECombatDamageResult::Invalid;
	if (TryResolveInvulnerability(
		Attempt,
		*TargetActor,
		*TargetAbilitySystemComponent,
		InvulnerabilityResult))
	{
		return BroadcastResolvedResult(Attempt, InvulnerabilityResult);
	}

	FGameplayEffectContextHandle EffectContext = SourceAbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(SourceActor);

	const FGameplayEffectSpecHandle EffectSpec =
	SourceAbilitySystemComponent->MakeOutgoingSpec(Attempt.DamageEffect, 1.0f, EffectContext);

	if (!EffectSpec.IsValid())
	{
		return ECombatDamageResult::Invalid;
	}

	if (Attempt.SetByCallerMagnitudeTag.IsValid())
	{
		EffectSpec.Data->SetSetByCallerMagnitude(
			Attempt.SetByCallerMagnitudeTag,
			Attempt.SetByCallerMagnitude
		);
	}

	const FActiveGameplayEffectHandle AppliedHandle =
	SourceAbilitySystemComponent->ApplyGameplayEffectSpecToTarget(*EffectSpec.Data.Get(), TargetAbilitySystemComponent);

	const ECombatDamageResult Result = AppliedHandle.WasSuccessfullyApplied()
		? ECombatDamageResult::Applied
		: ECombatDamageResult::Invalid;

	return BroadcastResolvedResult(Attempt, Result);
}

ECombatDamageResult UCombatDamageComponent::BroadcastResolvedResult(
	const FCombatDamageAttempt& Attempt,
	const ECombatDamageResult Result)
{
	if (Result != ECombatDamageResult::Invalid)
	{
		DamageResolvedEvent.Broadcast(Attempt, Result);
	}

	return Result;
}

bool UCombatDamageComponent::TryResolveInvulnerability(
	const FCombatDamageAttempt& Attempt,
	AActor& TargetActor,
	UAbilitySystemComponent& TargetAbilitySystemComponent,
	ECombatDamageResult& OutResult) const
{
	if (!InvulnerabilityTag.IsValid())
	{
		return false;
	}

	UCombatDefenseComponent* DefenseComponent = TargetActor.FindComponentByClass<UCombatDefenseComponent>();

	const bool bDodgeWindowOwnHitTime = DefenseComponent &&
		DefenseComponent->IsDodgeWindowActiveAt(InvulnerabilityTag, Attempt.HitTimeSeconds);

	const int32 DefenseOwnedTagCount =
		DefenseComponent && DefenseComponent->IsWindowTagApplied(InvulnerabilityTag)
			? 1
			: 0;

	// Independent invulnerability takes priority over the dodge window so a
	// later perfect-dodge reward cannot be granted while another source is
	// already protecting the target.
	if (TargetAbilitySystemComponent.GetTagCount(InvulnerabilityTag) > DefenseOwnedTagCount)
	{
		OutResult = ECombatDamageResult::OtherInvulnerable;
		return true;
	}

	if (!bDodgeWindowOwnHitTime)
	{
		return false;
	}

	if (Attempt.bCanTriggerPerfectDodge &&
		DefenseComponent->TryConsumePerfectDodge(InvulnerabilityTag, Attempt.HitTimeSeconds))
	{
		OutResult = ECombatDamageResult::PerfectDodge;
		return true;
	}

	OutResult = ECombatDamageResult::DodgeInvulnerable;
	return true;
}

void UCombatDamageComponent::ConfigureInvulnerabilityTag(const FGameplayTag& Tag)
{
	InvulnerabilityTag = Tag;
}

void UCombatDamageComponent::ConfigureTerminalStateTag(const FGameplayTag& Tag)
{
	TerminalStateTag = Tag;
}

UAbilitySystemComponent* UCombatDamageComponent::ResolveAbilitySystemComponent(AActor* Actor)
{
	// Depend on the public ASC contract rather than concrete player or enemy types.
	IAbilitySystemInterface* AbilitySystemOwner = Cast<IAbilitySystemInterface>(Actor);

	return AbilitySystemOwner ? AbilitySystemOwner->GetAbilitySystemComponent() : nullptr;
}
