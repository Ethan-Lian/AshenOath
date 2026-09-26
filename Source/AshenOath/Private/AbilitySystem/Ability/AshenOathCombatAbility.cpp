#include "AbilitySystem/Ability/AshenOathCombatAbility.h"

#include "AbilitySystem/AshenOathStaminaRecoveryComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/Data/AshenOathActionData.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayEffect.h"
#include "GameplayTags/AshenOathGameplayTags.h"

UAshenOathCombatAbility::UAshenOathCombatAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	bRetriggerInstancedAbility = false;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(AshenOathGameplayTags::Ability_Action);
	SetAssetTags(AssetTags);

	BlockAbilitiesWithTag.AddTag(AshenOathGameplayTags::Ability_Action);
	ActivationBlockedTags.AddTag(AshenOathGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(AshenOathGameplayTags::State_Staggered);
}

bool UAshenOathCombatAbility::CheckCost(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	const UAshenOathActionData* ActionData = ResolveActionData(Handle, ActorInfo);
	if (!ActionData)
	{
		return false;
	}

	if (!ActionData->CostEffect)
	{
		return true;
	}

	UAbilitySystemComponent* AbilitySystemComponent =
		ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const UGameplayEffect* CostEffect = ActionData->CostEffect.GetDefaultObject();

	if (!AbilitySystemComponent ||
		!CostEffect ||
		CostEffect->DurationPolicy != EGameplayEffectDurationType::Instant)
	{
		return false;
	}

	const bool bCanPayCost = AbilitySystemComponent->CanApplyAttributeModifiers(
		CostEffect,
		GetAbilityLevel(Handle, ActorInfo),
		MakeEffectContext(Handle, ActorInfo)
	);

	if (!bCanPayCost && OptionalRelevantTags)
	{
		const FGameplayTag& CostFailureTag =
			UAbilitySystemGlobals::Get().ActivateFailCostTag;

		if (CostFailureTag.IsValid())
		{
			OptionalRelevantTags->AddTag(CostFailureTag);
		}
	}

	return bCanPayCost;
}

void UAshenOathCombatAbility::ApplyCost(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	bCostApplicationSucceeded = false;
	const UAshenOathActionData* ActionData = ResolveActionData(Handle, ActorInfo);

	if (!ActionData)
	{
		return;
	}

	if (!ActionData->CostEffect)
	{
		bCostApplicationSucceeded = true;
		return;
	}

	const UGameplayEffect* CostEffect = ActionData->CostEffect.GetDefaultObject();
	if (!CostEffect || CostEffect->DurationPolicy != EGameplayEffectDurationType::Instant)
	{
		return;
	}

	const FGameplayEffectSpecHandle CostSpec = MakeOutgoingGameplayEffectSpec(
		Handle,
		ActorInfo,
		ActivationInfo,
		ActionData->CostEffect,
		GetAbilityLevel(Handle, ActorInfo)
	);

	if (!CostSpec.IsValid())
	{
		return;
	}

	const FActiveGameplayEffectHandle AppliedCost = ApplyGameplayEffectSpecToOwner(
		Handle,
		ActorInfo,
		ActivationInfo,
		CostSpec
	);

	if (!AppliedCost.WasSuccessfullyApplied())
	{
		return;
	}

	bCostApplicationSucceeded = true;
	UAbilitySystemComponent* AbilitySystemComponent =
		ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;

	// Applying a cost can synchronously add State.Dead and cancel this ability.
	// The death path stops recovery; never restart it on the way back out.
	if (!AbilitySystemComponent ||
		!IsValid(AvatarActor) ||
		AvatarActor->IsActorBeingDestroyed() ||
		AbilitySystemComponent->HasMatchingGameplayTag(
			AshenOathGameplayTags::State_Dead))
	{
		return;
	}

	if (UAshenOathStaminaRecoveryComponent* Recovery =
		AvatarActor->FindComponentByClass<UAshenOathStaminaRecoveryComponent>())
	{
		Recovery->NotifyStaminaCostCommitted();
	}
}

const UAshenOathActionData* UAshenOathCombatAbility::ResolveActionData(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo) const
{
	return Cast<UAshenOathActionData>(GetSourceObject(Handle, ActorInfo));
}

void UAshenOathCombatAbility::ResetCostApplicationResult()
{
	bCostApplicationSucceeded = false;
}

bool UAshenOathCombatAbility::DidCostApplicationSucceed() const
{
	return bCostApplicationSucceeded;
}
