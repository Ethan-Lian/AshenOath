#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Ability/AshenOathMeleeAttackAbility.h"
#include "AshenOathBossChargedSwingAbility.generated.h"

class UAshenOathBossChargedSwingActionData;

/** One Boss attack execution owns windup, charge, swing, and interruption. */
UCLASS()
class ASHENOATH_API UAshenOathBossChargedSwingAbility : public UAshenOathMeleeAttackAbility
{
	GENERATED_BODY()

public:
	UAshenOathBossChargedSwingAbility();

protected:
	virtual bool CanActivateAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags,
		const FGameplayTagContainer* TargetTags,
		FGameplayTagContainer* OptionalRelevantTags
	) const override;

	virtual void ActivateAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData
	) override;

	virtual void EndAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled
	) override;

private:
	bool IsChargedSwingDataReady(const UAshenOathBossChargedSwingActionData* ActionData) const;
	void BeginSwing();

	TWeakObjectPtr<const UAshenOathBossChargedSwingActionData> ActiveActionData;
	FTimerHandle ChargeTimer;
};
