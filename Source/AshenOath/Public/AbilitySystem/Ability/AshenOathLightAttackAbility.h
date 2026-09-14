#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Ability/AshenOathCombatAbility.h"
#include "Actions/CombatMeleeTypes.h"
#include "AshenOathLightAttackAbility.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UCombatActionData;
class UCombatMeleeComponent;

UCLASS()
class ASHENOATH_API UAshenOathLightAttackAbility : public UAshenOathCombatAbility
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

};
