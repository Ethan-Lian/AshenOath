#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Actions/CombatActionTypes.h"
#include "CombatActionComponent.generated.h"

class ACharacter;
class UAnimInstance;
class UAnimMontage;
class UCombatActionData;

/**
 * Owns the lifecycle of the Character's current combat action.
 *
 * The component allows at most one active action. An execution handle
 * distinguishes repeated executions of the same action and prevents stale
 * Montage callbacks from clearing a newer action.
 */

/** Data flow
  Input from player device
		↓
  PlayerController
		↓
  PlayerCharacter
		↓
  CombatActionComponent
		↓
  AnimInstance
		↓
  Play montage
*/


UCLASS(ClassGroup = (Combat),meta = (BlueprintSpawnableComponent))
class ASHENOATHCOMBAT_API UCombatActionComponent
	: public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatActionComponent();

	/**
	 * Attempts to start an action from the supplied configuration.
	 *
	 * On success, OutHandle identifies the new execution. On rejection,
	 * OutHandle is always invalid.
	 */
	ECombatActionStartResult TryStartAction(const UCombatActionData* ActionData,FCombatActionHandle& OutHandle);
	
	/**
	 * Cancels the current action and optionally blends its Montage out.
	 *
	 * BlendOutTime is the time used to return from the current Montage pose.
	 * A value of zero stops it immediately.
	 */
	void CancelCurrentAction(float BlendOutTime = 0.1f);

	bool IsActionActive() const;
	
protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// Weak references do not extend the lifetime of world-owned animation objects.
	TWeakObjectPtr<ACharacter> CachedCharacter;
	TWeakObjectPtr<UAnimInstance> ActiveAnimInstance;
	
	// Identifies the one action execution currently owned by this component.
	FCombatActionHandle CurrentAction;
	
	// Keep the active Montage visible to GC for the duration of the action.
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveMontage;
	
	// Zero is reserved for invalid handles.
	int32 NextActionId = 1;

	int32 AllocateActionId();
	
	// Receives the engine-supplied Montage result and the action ID captured 
	// when the callback was registered.
	void HandleMontageEnded(UAnimMontage* Montage,bool bInterrupted,int32 ExpectedActionId);

	// Shared idempotent cleanup for natural completion and active cancellation.
	void FinishAction(int32 ExpectedActionId,bool bStopMontage,float BlendOutTime);
};
