#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Actions/CombatActionTypes.h"
#include "CombatActionComponent.generated.h"

class AActor;
class ACharacter;
class UAnimInstance;
class UAnimMontage;
class UCombatActionData;
class UGameplayEffect;

/**
 * Owns the runtime lifecycle of the Character's current combat action.
 *
 * At most one action may be active at a time. The component owns the action's
 * Montage, melee hit window, damage snapshot, trace history, and per-window
 * hit records.
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
	void TickMeleeHitWindow();

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

	void ResetActiveActionDamageData();

	void ResetMeleeHitWindow();

	// Sweeps one melee trace segment and submits valid hit actors.
	void SweepMeleeSegment(const FVector& Start, const FVector& End);

	// Converts one detected actor into a validated combat damage attempt
	void SubmitMeleeHit(AActor* HitActor);

	// Weak references do not extend the lifetime of world-owned animation objects.
	TWeakObjectPtr<ACharacter> CachedCharacter;
	TWeakObjectPtr<UAnimInstance> ActiveAnimInstance;

	// Keep the active Montage visible to GC for the duration of the action.
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveMontage;

	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> ActiveDamageEffect;

	// Identifies the one action execution currently owned by this component.
	FCombatActionHandle CurrentAction;
	
	// Zero is reserved for invalid handles.
	int32 NextActionInstanceId = 1;

	// Identifies which action and Notify State currently own the hit window.
	int32 ActiveHitWindowActionInstanceId = 0;
	int32 ActiveHitWindowNotifyInstanceId = INDEX_NONE;

	// Damage segment associated with the current hit window.
	int32 ActiveDamageSegmentId = 0;

	// Ordered weapon sample points used for melee tracing.
	TArray<FName> ActiveMeleeTraceBones;

	// Previous-frame world positions of each melee trace sample point.
	TArray<FVector> PreviousMeleeTraceLocations;

	// Prevents the same actor from being damaged repeatedly within one hit window.
	TSet<TWeakObjectPtr<AActor>> HitActorsInCurrentWindow;
	
	float ActiveMeleeTraceRadius = 0.0f;

	bool bActiveDamageCanTriggerPerfectDodge = false;
};
