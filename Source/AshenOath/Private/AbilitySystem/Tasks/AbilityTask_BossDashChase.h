#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_BossDashChase.generated.h"

class ACharacter;
class UCharacterMovementComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBossDashChaseTaskEvent);

/** Steers a Character toward a moving target at an increased walking speed. */
UCLASS()
class UAbilityTask_BossDashChase : public UAbilityTask
{
	GENERATED_BODY()

public:
	UAbilityTask_BossDashChase(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(BlueprintAssignable)
	FBossDashChaseTaskEvent OnReached;

	UPROPERTY(BlueprintAssignable)
	FBossDashChaseTaskEvent OnFailed;

	static UAbilityTask_BossDashChase* ChaseTarget(
		UGameplayAbility* OwningAbility,
		FName TaskInstanceName,
		AActor* TargetActor,
		float StopDistance,
		float SpeedMultiplier
	);

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;
	virtual void OnDestroy(bool bInOwnerFinished) override;

private:
	void FinishChase(bool bReached);
	void ReleaseMovement();

	TWeakObjectPtr<AActor> Target;
	TWeakObjectPtr<ACharacter> Character;
	TWeakObjectPtr<UCharacterMovementComponent> MovementComponent;
	FVector ProgressCheckLocation = FVector::ZeroVector;
	float StopDistance = 0.0f;
	float SpeedMultiplier = 1.0f;
	float SavedMaxWalkSpeed = 0.0f;
	float ProgressCheckElapsed = 0.0f;
	bool bOwnsSpeedOverride = false;
	bool bChaseFinished = false;
};
