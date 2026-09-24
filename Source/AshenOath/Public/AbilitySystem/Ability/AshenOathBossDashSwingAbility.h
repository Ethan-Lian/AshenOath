#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Ability/AshenOathMeleeAttackAbility.h"
#include "AshenOathBossDashSwingAbility.generated.h"

class UAbilityTask_ApplyCombatMovement;
class UAshenOathBossDashSwingActionData;

/** One Boss attack execution owns the dash and its following swing. */
UCLASS()
class ASHENOATH_API UAshenOathBossDashSwingAbility : public UAshenOathMeleeAttackAbility
{
	GENERATED_BODY()

public:
	UAshenOathBossDashSwingAbility();

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
	bool IsDashSwingDataReady(const UAshenOathBossDashSwingActionData* ActionData) const;

	UFUNCTION()
	void HandleDashCompleted();

	UFUNCTION()
	void HandleDashFailed();

	TWeakObjectPtr<const UAshenOathBossDashSwingActionData> ActiveActionData;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_ApplyCombatMovement> MovementTask;
};
