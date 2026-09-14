#pragma once

#include "CoreMinimal.h"
#include "Actions/CombatMeleeTypes.h"
#include "Components/ActorComponent.h"
#include "CombatMeleeComponent.generated.h"

class AActor;
class ACharacter;
class UAnimInstance;
class UAnimMontage;
class UAnimSequenceBase;
class UGameplayEffect;
class USkeletalMeshComponent;

/**
 * Owns the mutable detection state for one melee session.
 *
 * The lifecycle owner (normally a GameplayAbility) supplies an immutable combat
 * snapshot and the exact Montage playback identity. Animation notifications are
 * then accepted only from that Mesh, AnimInstance and Montage instance, so a
 * delayed callback from an older execution cannot operate a newer session.
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

	bool CanStartSession(
		TSubclassOf<UGameplayEffect> DamageEffect,
		float TraceRadius,
		const TArray<FName>& TraceBones,
		const USkeletalMeshComponent* SourceMesh,
		const UAnimInstance* SourceAnimInstance
	) const;

	FCombatMeleeSessionHandle BeginSession(
		TSubclassOf<UGameplayEffect> DamageEffect,
		float TraceRadius,
		const TArray<FName>& TraceBones,
		bool bCanTriggerPerfectDodge,
		USkeletalMeshComponent* SourceMesh,
		UAnimInstance* SourceAnimInstance,
		UAnimMontage* SourceMontage,
		int32 MontageInstanceId
	);

	void EndSession(const FCombatMeleeSessionHandle& SessionHandle);
	bool IsSessionActive(const FCombatMeleeSessionHandle& SessionHandle) const;
	bool HasActiveSession() const;

	// Animation-facing entry points. MontageInstanceId must come from the notify
	// event context (or branching-point payload), never from the current Ability.
	void BeginHitWindowFromAnimation(
		USkeletalMeshComponent* MeshComponent,
		UAnimSequenceBase* Animation,
		int32 MontageInstanceId,
		int32 NotifyInstanceId,
		int32 DamageSegmentId
	);

	void EndHitWindowFromAnimation(
		USkeletalMeshComponent* MeshComponent,
		UAnimSequenceBase* Animation,
		int32 MontageInstanceId,
		int32 NotifyInstanceId
	);

	void TickHitWindowFromAnimation(
		USkeletalMeshComponent* MeshComponent,
		UAnimSequenceBase* Animation,
		int32 MontageInstanceId,
		int32 NotifyInstanceId
	);

	bool IsHitWindowActive(const FCombatMeleeSessionHandle& SessionHandle) const;
	bool HasActiveHitWindow() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	int32 AllocateSessionInstanceId();
	void ResetSession();

	void BeginHitWindow(
		const FCombatMeleeSessionHandle& SessionHandle,
		int32 NotifyInstanceId,
		int32 DamageSegmentId
	);

	void EndHitWindow(
		const FCombatMeleeSessionHandle& SessionHandle,
		int32 NotifyInstanceId
	);

	void TickHitWindow(
		const FCombatMeleeSessionHandle& SessionHandle,
		int32 NotifyInstanceId
	);

	// Clears per-window trace history and hit deduplication without ending the session.
	void ResetHitWindow();

	// Sweeps one melee trace segment and submits valid hit actors.
	void SweepMeleeSegment(
		const FCombatMeleeSessionHandle& SessionHandle,
		int32 NotifyInstanceId,
		const FVector& Start,
		const FVector& End
	);

	// Converts one detected actor into a target-side combat damage attempt.
	void SubmitMeleeHit(
		const FCombatMeleeSessionHandle& SessionHandle,
		int32 NotifyInstanceId,
		AActor* HitActor
	);

	bool OwnsHitWindow(
		const FCombatMeleeSessionHandle& SessionHandle,
		int32 NotifyInstanceId
	) const;

	bool IsAnimationSignalOwned(
		const USkeletalMeshComponent* MeshComponent,
		const UAnimSequenceBase* Animation,
		int32 MontageInstanceId,
		bool bRequireActiveMontage
	) const;

	FCombatMeleeSessionHandle GetActiveSessionHandle() const;

	// Weak ownership avoids extending the lifetime of world-owned animation objects.
	TWeakObjectPtr<ACharacter> CachedCharacter;
	TWeakObjectPtr<USkeletalMeshComponent> ActiveSourceMesh;
	TWeakObjectPtr<UAnimInstance> ActiveSourceAnimInstance;
	TWeakObjectPtr<UAnimMontage> ActiveSourceMontage;

	// Immutable snapshot copied from the action configuration at session start.
	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> ActiveDamageEffect;

	TArray<FName> ActiveMeleeTraceBones;
	float ActiveMeleeTraceRadius = 0.0f;
	bool bActiveDamageCanTriggerPerfectDodge = false;

	// Identifies which detection session owns the current melee snapshot.
	int32 ActiveSessionInstanceId = 0;
	int32 ActiveMontageInstanceId = INDEX_NONE;

	// Zero is reserved for an invalid session.
	int32 NextSessionInstanceId = 1;

	// Identifies the Notify State and damage segment that own the open window.
	int32 ActiveHitWindowNotifyInstanceId = INDEX_NONE;
	int32 ActiveDamageSegmentId = 0;

	// Previous-frame world positions of each ordered weapon sample point.
	TArray<FVector> PreviousMeleeTraceLocations;

	// Prevents the same actor from being damaged repeatedly within one hit window.
	TSet<TWeakObjectPtr<AActor>> HitActorsInCurrentWindow;
};
