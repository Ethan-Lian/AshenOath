#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Ability/AshenOathCombatAbility.h"
#include "AshenOathHealAbility.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAnimInstance;
class UAnimMontage;
class UAnimSequenceBase;
class UAshenOathHealActionData;
class AAshenOathPlayerCharacter;
class USkeletalMeshComponent;

/** Owns one stationary Cast execution; the learner completes the Notify transaction. */
UCLASS()
class ASHENOATH_API UAshenOathHealAbility : public UAshenOathCombatAbility
{
	GENERATED_BODY()

public:
	UAshenOathHealAbility();

	// Accepts only the Mesh and Montage instance started by this execution.
	bool TryCommitHealFromAnimation(
		USkeletalMeshComponent* MeshComponent,
		UAnimSequenceBase* Animation,
		int32 MontageInstanceId
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
	bool IsHealActionDataReady(const UAshenOathHealActionData* ActionData) const;
	bool IsHealSignalOwned(
		const USkeletalMeshComponent* MeshComponent,
		const UAnimSequenceBase* Animation,
		int32 MontageInstanceId
	) const;
	bool ApplyConfiguredHealEffect();
	void FinishAbility(bool bWasCancelled);

	UFUNCTION()
	void HandleMontageCompleted();

	UFUNCTION()
	void HandleMontageInterrupted();

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	TWeakObjectPtr<AAshenOathPlayerCharacter> ActivePlayer;
	TWeakObjectPtr<const UAshenOathHealActionData> ActiveHealData;
	TWeakObjectPtr<USkeletalMeshComponent> ActiveSourceMesh;
	TWeakObjectPtr<UAnimInstance> ActiveSourceAnimInstance;
	TWeakObjectPtr<UAnimMontage> ActiveSourceMontage;
	int32 ActiveMontageInstanceId = INDEX_NONE;
	bool bHealCommitAttempted = false;
};
