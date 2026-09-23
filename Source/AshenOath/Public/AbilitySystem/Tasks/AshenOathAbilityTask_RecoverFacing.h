#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AshenOathAbilityTask_RecoverFacing.generated.h"

/**
 * Smoothly returns an Ability avatar toward a live target near action handoff.
 *
 * The target bearing is sampled every tick so moving targets do not leave the
 * avatar recovering toward an obsolete direction.
 */
UCLASS()
class ASHENOATH_API UAshenOathAbilityTask_RecoverFacing : public UAbilityTask
{
	GENERATED_BODY()

public:
	UAshenOathAbilityTask_RecoverFacing(const FObjectInitializer& ObjectInitializer);

	// Recovers from StartOffsetSeconds after execution start for DurationSeconds.
	// ExecutionStartWorldTime must use the owning UWorld clock; the task ends if
	// either actor becomes invalid.
	static UAshenOathAbilityTask_RecoverFacing* RecoverFacing(
		UGameplayAbility* OwningAbility,
		FName TaskInstanceName,
		AActor* FacingTarget,
		float StartOffsetSeconds,
		float DurationSeconds,
		double ExecutionStartWorldTime
	);

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;
	virtual void OnDestroy(bool bInOwnerFinished) override;

private:
	TWeakObjectPtr<AActor> AvatarActor;
	TWeakObjectPtr<AActor> FacingTarget;

	float RecoveryStartOffset = 0.0f;
	float RecoveryDuration = 0.0f;
	float RecoveryStartYaw = 0.0f;
	double ExecutionStartTime = 0.0;
	bool bRecoveryStarted = false;
};
