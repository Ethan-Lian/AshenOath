#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Ability/AshenOathMeleeAttackAbility.h"
#include "Feedback/AshenOathChargeState.h"
#include "AshenOathHeavyAttackAbility.generated.h"

class UAshenOathHeavyAttackData;
class UAshenOathCombatFeedbackComponent;

enum class EAshenOathHeavyAttackPhase : uint8
{
	None,
	Charging,
	ChargedRelease
};

/** Owns the complete press/hold/release gesture for heavy attacks. */
UCLASS()
class ASHENOATH_API UAshenOathHeavyAttackAbility : public UAshenOathMeleeAttackAbility
{
	GENERATED_BODY()

public:
	UAshenOathHeavyAttackAbility();

	// Releases the stationary staff thrust after at least one second of charging;
	// an earlier release cancels the charge and returns false.
	bool HandleInputReleased();

	// Dodge may preempt only the undecided/charging portion of this gesture.
	bool CancelChargeForDodge();
	FAshenOathChargeState GetChargeState() const;
	void RequestReleaseFeedbackFromAnimation(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation, int32 MontageInstanceId);

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
	bool IsHeavyDataReady(const UAshenOathHeavyAttackData* HeavyData) const;
	bool StartCharging(
		const FGameplayAbilityActorInfo* ActorInfo,
		const UAshenOathHeavyAttackData* HeavyData,
		UWorld* World
	);
	bool StartChargedRelease();
	bool ConsumeChargeThroughCurrentTime();
	float CalculateChargedDamageMagnitude() const;
	float GetChargeAlpha() const;
	void PublishChargeState();
	void AlignChargedReleaseToActiveBoss();
	void AcquireMovementLock();
	void ReleaseMovementLock();

	void HandleChargeDrainTick();
	void HandleMaximumChargeReached();

	TWeakObjectPtr<const UAshenOathHeavyAttackData> ActiveHeavyData;

	FTimerHandle ChargeDrainTimer;
	FTimerHandle MaximumChargeTimer;
	FTimerHandle ChargeReadyTimer;
	TWeakObjectPtr<UAshenOathCombatFeedbackComponent> ChargeFeedbackComponent;
	uint32 ChargeFeedbackId = 0;
	bool bReleaseFeedbackPlayed = false;
	EAshenOathHeavyAttackPhase ActivePhase = EAshenOathHeavyAttackPhase::None;
	double ChargeStartedAtSeconds = 0.0;
	float AccumulatedChargeStaminaCost = 0.0f;
	bool bOwnsMovementLock = false;
};
