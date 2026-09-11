#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Actions/CombatActionTypes.h"
#include "CombatActionComponent.generated.h"

class ACharacter;
class UAnimInstance;
class UAnimMontage;
class UCombatActionData;
class UCombatMeleeComponent;

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

	/**
	 * Attempts to start an action from the supplied configuration.
	 *
	 * On success, OutHandle identifies the new execution.
	 * On rejection, OutHandle is always invalid.
	 */
	ECombatActionStartResult TryStartAction(const UCombatActionData* ActionData, FCombatActionHandle& OutHandle);

	/**
	 * Cancels the current action and optionally blends its Montage out.
	 *
	 * BlendOutTime is the time used to return from the current Montage pose.
	 * A value of zero stops it immediately.
	 */
	void CancelCurrentAction(float BlendOutTime = 0.1f);

	bool IsActionActive() const;

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

	/**
	 * Handles Montage completion using the action ID captured when the
	 * callback was registered.
	 */
	void HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted, int32 ExpectedActionId);

	// Shared idempotent cleanup for natural completion and active cancellation.
	void FinishAction(int32 ExpectedActionId, bool bStopMontage, float BlendOutTime);

	// Weak references do not extend the lifetime of world-owned runtime objects.
	TWeakObjectPtr<ACharacter> CachedCharacter;
	TWeakObjectPtr<UAnimInstance> ActiveAnimInstance;
	TWeakObjectPtr<UCombatMeleeComponent> MeleeComponent;

	// Keep the active Montage visible to GC for the duration of the action.
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveMontage;

	// Identifies the one action execution currently owned by this component.
	FCombatActionHandle CurrentAction;

	// Zero is reserved for invalid handles.
	int32 NextActionInstanceId = 1;
};
