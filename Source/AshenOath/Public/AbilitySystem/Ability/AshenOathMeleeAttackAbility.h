#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Ability/AshenOathCombatAbility.h"
#include "Actions/CombatMeleeTypes.h"
#include "AshenOathMeleeAttackAbility.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAnimInstance;
class UAnimMontage;
class UAnimSequenceBase;
class UAshenOathMeleeActionData;
class UCombatMeleeComponent;
class USkeletalMeshComponent;

/**
 * Shared execution lifecycle for Montage-driven melee abilities.
 *
 * Concrete attacks decide how and when to start playback. This base owns the
 * exact animation identity, melee session, cost transaction, and cleanup.
 */
UCLASS(Abstract)
class ASHENOATH_API UAshenOathMeleeAttackAbility : public UAshenOathCombatAbility
{
	GENERATED_BODY()

protected:
	virtual bool CanActivateAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags,
		const FGameplayTagContainer* TargetTags,
		FGameplayTagContainer* OptionalRelevantTags
	) const override;

	virtual void EndAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled
	) override;

	// Starts playback and captures the exact animation source for this execution.
	// No melee session or cost is committed until BeginMeleeExecution succeeds.
	bool StartAttackMontage(
		const FGameplayAbilityActorInfo* ActorInfo,
		const UAshenOathMeleeActionData* ActionData,
		FName InitialSection
	);

	// Attaches damage and optional cost ownership to the active Montage.
	bool BeginMeleeExecution(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		const UAshenOathMeleeActionData* ActionData,
		bool bCommitConfiguredCost,
		FGameplayTag SetByCallerMagnitudeTag = FGameplayTag(),
		float SetByCallerMagnitude = 0.0f
	);

	bool IsMeleeActionDataReady(
		const UAshenOathMeleeActionData* ActionData,
		const FGameplayAbilityActorInfo* ActorInfo
	) const;

	bool IsAttackSignalOwned(
		const USkeletalMeshComponent* MeshComponent,
		const UAnimSequenceBase* Animation,
		int32 MontageInstanceId
	) const;

	UAnimInstance* GetActiveAttackAnimInstance() const;
	UAnimMontage* GetActiveAttackMontage() const;
	int32 GetActiveAttackMontageInstanceId() const;
	void FinishAbility(bool bWasCancelled);

private:
	UCombatMeleeComponent* ResolveMeleeComponent(
		const FGameplayAbilityActorInfo* ActorInfo
	) const;

	UFUNCTION()
	void HandleMontageCompleted();

	UFUNCTION()
	void HandleMontageInterrupted();

	void ResetMeleeExecutionState();

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	TWeakObjectPtr<UCombatMeleeComponent> ActiveMeleeComponent;
	FCombatMeleeSessionHandle ActiveMeleeSession;

	TWeakObjectPtr<USkeletalMeshComponent> ActiveSourceMesh;
	TWeakObjectPtr<UAnimInstance> ActiveSourceAnimInstance;
	TWeakObjectPtr<UAnimMontage> ActiveSourceMontage;
	int32 ActiveMontageInstanceId = INDEX_NONE;
};
