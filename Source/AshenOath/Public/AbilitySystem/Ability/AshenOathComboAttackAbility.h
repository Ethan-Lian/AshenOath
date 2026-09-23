#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Ability/AshenOathMeleeAttackAbility.h"
#include "AshenOathComboAttackAbility.generated.h"

class UAnimSequenceBase;
class UCombatActionData;
class USkeletalMeshComponent;

/** Owns the player's light-combo input, section transitions, and execution. */
UCLASS()
class ASHENOATH_API UAshenOathComboAttackAbility : public UAshenOathMeleeAttackAbility
{
	GENERATED_BODY()

public:
	UAshenOathComboAttackAbility();

	// Called when the combo-attack input repeats while this Ability is active.
	bool TryQueueComboInput();

	// Signals are accepted only from the Mesh, Montage instance, and notify
	// instance owned by this execution.
	void BeginComboWindowFromAnimation(
		USkeletalMeshComponent* MeshComponent,
		UAnimSequenceBase* Animation,
		int32 MontageInstanceId,
		int32 NotifyInstanceId
	);

	void EndComboWindowFromAnimation(
		USkeletalMeshComponent* MeshComponent,
		UAnimSequenceBase* Animation,
		int32 MontageInstanceId,
		int32 NotifyInstanceId
	);

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
	bool IsComboActionDataReady(const UCombatActionData* ActionData) const;

	bool StartComboExecution(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		const UCombatActionData* ActionData,
		FName InitialSection
	);

	void ResetComboState();

	TArray<FName> ActiveComboSections;
	int32 ActiveComboWindowNotifyInstanceId = INDEX_NONE;
	bool bComboWindowOpen = false;
	bool bComboInputConsumedInWindow = false;
};
