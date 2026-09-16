#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "AshenOathCombatAbility.generated.h"

class UCombatActionData;

/**
 * Thin shared contract for combat abilities backed by UCombatActionData.
 *
 * Concrete abilities still own their distinct execution flow. This base only
 * centralizes action mutual exclusion and the single-source GAS cost transaction.
 */
UCLASS(Abstract)
class ASHENOATH_API UAshenOathCombatAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UAshenOathCombatAbility();

protected:
	virtual bool CheckCost(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags = nullptr
	) const override;

	virtual void ApplyCost(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo
	) const override;

	const UCombatActionData* ResolveActionData(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo
	) const;

	void ResetCostApplicationResult();
	bool DidCostApplicationSucceed() const;

private:
	// GAS exposes ApplyCost as const. InstancedPerActor makes this flag local to
	// one granted spec and it is reset before every CommitAbility call.
	mutable bool bCostApplicationSucceeded = false;
};
