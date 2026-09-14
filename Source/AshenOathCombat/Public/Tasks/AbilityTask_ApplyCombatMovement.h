#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_ApplyCombatMovement.generated.h"

class ACharacter;
class UAnimInstance;
class UCharacterMovementComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCombatMovementTaskFailed);

/**
 * Applies one collision-aware, code-driven displacement for an Ability.
 *
 * The task temporarily owns CharacterMovement's movement mode and the animation
 * root-motion mode. Ability termination destroys the task and restores both.
 */
UCLASS()
class ASHENOATHCOMBAT_API UAbilityTask_ApplyCombatMovement : public UAbilityTask
{
	GENERATED_BODY()

public:
	UAbilityTask_ApplyCombatMovement(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(BlueprintAssignable)
	FCombatMovementTaskFailed OnFailed;

	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks",
		meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityTask_ApplyCombatMovement* ApplyCombatMovement(
		UGameplayAbility* OwningAbility,
		FName TaskInstanceName,
		FVector WorldDirection,
		float Distance,
		float StartOffsetSeconds,
		float DurationSeconds,
		double ExecutionStartWorldTime
	);

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;
	virtual void OnDestroy(bool bInOwnerFinished) override;

private:
	void BeginMovementControl();
	void EndMovementControl();
	void RestoreRootMotionMode();
	void FailTask();

	TWeakObjectPtr<ACharacter> Character;
	TWeakObjectPtr<UCharacterMovementComponent> MovementComponent;
	TWeakObjectPtr<UAnimInstance> AnimInstance;
	FVector MovementDirection = FVector::ZeroVector;
	float MovementDistance = 0.0f;
	float MovementStartOffset = 0.0f;
	float MovementDuration = 0.0f;
	double ExecutionStartTime = 0.0;
	float PreviousMovementAlpha = 0.0f;
	uint8 SavedMovementMode = 0;
	uint8 SavedCustomMovementMode = 0;
	uint8 SavedRootMotionMode = 0;
	bool bMovementControlActive = false;
	bool bRootMotionModeOverridden = false;
	bool bMovementFinished = false;
};
