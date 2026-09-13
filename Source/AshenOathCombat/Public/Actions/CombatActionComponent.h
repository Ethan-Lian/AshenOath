#pragma once

#include "CoreMinimal.h"
#include "Actions/CombatActionTypes.h"
#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "CombatActionComponent.generated.h"

class ACharacter;
class UAbilitySystemComponent;
class UAnimInstance;
class UAnimMontage;
class UCombatActionData;
class UCombatMeleeComponent;
class UGameplayEffect;

/**
 * Owns the runtime lifecycle of the Character's current combat action.
 *
 * At most one action may be active at a time. The component owns the action's
 * handle and Montage, and coordinates action-scoped systems such as melee hit
 * detection. UCombatMeleeComponent owns the mutable melee runtime state.
 *
 * Execution IDs distinguish repeated executions of the same action and prevent
 * stale Montage or Notify callbacks from modifying a newer action.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class ASHENOATHCOMBAT_API UCombatActionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatActionComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * Attempts to start an action from the supplied configuration.
	 *
	 * On success, OutHandle identifies the new execution.
	 * On rejection, OutHandle is always invalid.
	 */
	ECombatActionStartResult TryStartAction(const UCombatActionData* ActionData, FCombatActionHandle& OutHandle);

	// MovementDirection is a world-space request snapshot. Actions without
	// configured movement ignore it.
	ECombatActionStartResult TryStartAction(const UCombatActionData* ActionData,
	                                       const FVector& MovementDirection,
	                                       FCombatActionHandle& OutHandle);

	/**
	 * Cancels the current action and optionally blends its Montage out.
	 *
	 * BlendOutTime is the time used to return from the current Montage pose.
	 * A value of zero stops it immediately.
	 */
	void CancelCurrentAction(float BlendOutTime = 0.1f);

	bool IsActionActive() const;

	// Supplies a project-specific periodic recovery effect without making the
	// reusable Combat module depend on a concrete stamina attribute.
	void ConfigureResourceRecovery(TSubclassOf<UGameplayEffect> RecoveryEffect, float DelaySeconds);

	// Stops the recovery timer/effect owned by this component. Other active
	// effects on the ASC are left untouched.
	void StopResourceRecovery();
	bool HasPendingOrActiveResourceRecovery() const;

	// Returns true only when the current action owns Tag at the supplied world
	// time. This distinguishes dodge invulnerability from unrelated sources.
	bool IsGameplayTagWindowActiveAt(const FGameplayTag& Tag, double WorldTimeSeconds) const;
	bool IsGameplayTagWindowApplied(const FGameplayTag& Tag) const;

	/**
	 * Opens the melee hit window owned by the current action.
	 *
	 * NotifyInstanceId identifies this Notify State execution so delayed
	 * NotifyEnd calls from older executions cannot close a newer hit window.
	 */
	void BeginMeleeHitWindow(int32 NotifyInstanceId, int32 DamageSegmentId);

	// Closes the hit window only when the Notify State execution still owns it.
	void EndMeleeHitWindow(int32 NotifyInstanceId);

	// Called by the active AnimNotifyState after animation evaluation.
	void TickMeleeHitWindow(int32 NotifyInstanceId);

	bool IsMeleeHitWindowActive() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	int32 AllocateActionInstanceId();
	UAbilitySystemComponent* ResolveAbilitySystemComponent() const;
	bool IsActionDataValid(const UCombatActionData& ActionData, const FVector& MovementDirection) const;

	/**
	 * Handles Montage completion using the action ID captured when the
	 * callback was registered.
	 */
	void HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted, int32 ExpectedActionId);

	// Shared idempotent cleanup for natural completion and active cancellation.
	void FinishAction(int32 ExpectedActionId, bool bStopMontage, float BlendOutTime);

	void InitializeActionRuntime(const UCombatActionData& ActionData, const FVector& MovementDirection);
	void UpdateActionRuntime(float PreviousElapsedTime, float NewElapsedTime);
	void ClearActionRuntime();
	void BeginMovementControl();
	void EndMovementControl();
	void ActivateWindowTags();
	void DeactivateWindowTags();
	void RestartResourceRecovery();
	void StartResourceRecovery();

	// Weak references do not extend the lifetime of world-owned runtime objects.
	TWeakObjectPtr<ACharacter> CachedCharacter;
	TWeakObjectPtr<UAnimInstance> ActiveAnimInstance;
	TWeakObjectPtr<UCombatMeleeComponent> MeleeComponent;

	// Keep the active Montage visible to GC for the duration of the action.
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveMontage;

	// Identifies the one action execution currently owned by this component.
	FCombatActionHandle CurrentAction;

	// Action data is copied into runtime state so editing a shared Data Asset
	// cannot mutate an in-flight action.
	FVector ActiveMovementDirection = FVector::ZeroVector;
	float ActiveMovementDistance = 0.0f;
	float ActiveMovementStartTime = 0.0f;
	float ActiveMovementDuration = 0.0f;
	float ActiveWindowStartTime = 0.0f;
	float ActiveWindowDuration = 0.0f;
	float ActionElapsedTime = 0.0f;
	double ActionStartWorldTime = 0.0;
	FGameplayTagContainer ConfiguredWindowTags;
	FGameplayTagContainer AppliedWindowTags;
	bool bMovementControlActive = false;
	bool bRootMotionModeOverridden = false;
	uint8 SavedMovementMode = 0;
	uint8 SavedCustomMovementMode = 0;
	uint8 SavedRootMotionMode = 0;

	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> ResourceRecoveryEffect;

	float ResourceRecoveryDelay = 0.0f;
	FTimerHandle ResourceRecoveryTimer;
	FActiveGameplayEffectHandle ActiveRecoveryEffect;

	// Zero is reserved for invalid handles.
	int32 NextActionInstanceId = 1;
};
