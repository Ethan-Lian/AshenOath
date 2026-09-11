#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Actions/CombatActionTypes.h"
#include "CombatMeleeComponent.generated.h"

class AActor;
class ACharacter;
class UGameplayEffect;

/**
 * Owns the runtime state used to detect melee hits for one combat action.
 *
 * UCombatActionComponent owns the action lifecycle and supplies a snapshot of
 * the action's melee configuration. Animation hit-window notifications are
 * routed through the action component so stale callbacks can be checked against
 * the active action handle before this component samples the weapon.
 *
 * This component detects and deduplicates hit actors. The target actor's
 * UCombatDamageComponent remains responsible for validating and applying damage.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class ASHENOATHCOMBAT_API UCombatMeleeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatMeleeComponent();

	/**
	 * Captures the melee configuration for one action execution.
	 *
	 * The values are copied because the Data Asset describes shared configuration,
	 * while this component owns mutable state for the active execution.
	 */
	void BeginAction(const FCombatActionHandle& ActionHandle,
	                 TSubclassOf<UGameplayEffect> DamageEffect,
	                 float TraceRadius,
	                 const TArray<FName>& TraceBones,
	                 bool bCanTriggerPerfectDodge);

	// Releases melee state only when the caller still owns the active action.
	void EndAction(const FCombatActionHandle& ActionHandle);

	void BeginHitWindow(const FCombatActionHandle& ActionHandle,
	                    int32 NotifyInstanceId,
	                    int32 DamageSegmentId);

	// Closes the window only when both its action and Notify State still match.
	void EndHitWindow(const FCombatActionHandle& ActionHandle, int32 NotifyInstanceId);

	// Samples and sweeps the weapon after animation evaluation for the current frame.
	void TickHitWindow(const FCombatActionHandle& ActionHandle, int32 NotifyInstanceId);

	bool IsHitWindowActive(const FCombatActionHandle& ActionHandle) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool HasActiveHitWindow() const;

	// Clears the execution snapshot and any window owned by that execution.
	void ResetAction();

	// Clears per-window trace history and hit deduplication without ending the action.
	void ResetHitWindow();

	// Sweeps one melee trace segment and submits valid hit actors.
	void SweepMeleeSegment(const FCombatActionHandle& ActionHandle,
	                       int32 NotifyInstanceId,
	                       const FVector& Start,
	                       const FVector& End);

	// Converts one detected actor into a target-side combat damage attempt.
	void SubmitMeleeHit(const FCombatActionHandle& ActionHandle,
	                    int32 NotifyInstanceId,
	                    AActor* HitActor);

	bool OwnsHitWindow(const FCombatActionHandle& ActionHandle, int32 NotifyInstanceId) const;

	// Weak ownership avoids extending the lifetime of the world-owned character.
	TWeakObjectPtr<ACharacter> CachedCharacter;

	// Immutable snapshot copied from the action configuration at action start.
	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> ActiveDamageEffect;

	TArray<FName> ActiveMeleeTraceBones;
	float ActiveMeleeTraceRadius = 0.0f;
	bool bActiveDamageCanTriggerPerfectDodge = false;

	// Identifies which action execution owns the current melee snapshot.
	int32 ActiveActionInstanceId = 0;

	// Identifies the Notify State and damage segment that own the open window.
	int32 ActiveHitWindowNotifyInstanceId = INDEX_NONE;
	int32 ActiveDamageSegmentId = 0;

	// Previous-frame world positions of each ordered weapon sample point.
	TArray<FVector> PreviousMeleeTraceLocations;

	// Prevents the same actor from being damaged repeatedly within one hit window.
	TSet<TWeakObjectPtr<AActor>> HitActorsInCurrentWindow;
};
