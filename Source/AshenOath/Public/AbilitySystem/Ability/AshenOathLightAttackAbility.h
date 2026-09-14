#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Actions/CombatMeleeTypes.h"
#include "AshenOathLightAttackAbility.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UCombatActionData;
class UCombatMeleeComponent;

UCLASS()
class ASHENOATH_API UAshenOathLightAttackAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UAshenOathLightAttackAbility();

protected:
	virtual bool CanActivateAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags,
		const FGameplayTagContainer* TargetTags,
		FGameplayTagContainer* OptionalRelevantTags
	) const override;

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
	const UCombatActionData* ResolveActionData(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo
	) const;

	bool IsActionDataReady(
		const UCombatActionData* ActionData,
		const FGameplayAbilityActorInfo* ActorInfo
	) const;

	UCombatMeleeComponent* ResolveMeleeComponent(
		const FGameplayAbilityActorInfo* ActorInfo
	) const;

	UFUNCTION()
	void HandleMontageCompleted();

	UFUNCTION()
	void HandleMontageInterrupted();

	void FinishAbility(bool bWasCancelled);

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	TWeakObjectPtr<UCombatMeleeComponent> ActiveMeleeComponent;
	FCombatMeleeSessionHandle ActiveMeleeSession;

	// ApplyCost is const in GAS, but this flag records the result of the current
	// InstancedPerActor execution. It is reset immediately before every commit.
	mutable bool bCostApplicationSucceeded = false;
};
