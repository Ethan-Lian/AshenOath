#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "CombatTargetingComponent.generated.h"

/**
 * Owns one combat target and the generic rules that keep that target valid.
 *
 * The component does not discover project-specific candidates, read input,
 * move the owner, or rotate a camera. Callers inject project tags and consume
 * target state through the query/event interface.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class ASHENOATHCOMBAT_API UCombatTargetingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatTargetingComponent();

	DECLARE_MULTICAST_DELEGATE_TwoParams(
		FTargetChangedEvent,
		AActor* /* PreviousTarget */,
		AActor* /* NewTarget */
	);

	// Optional project tag used to reject terminal targets; an invalid tag disables
	// this state check.
	void ConfigureTerminalStateTag(const FGameplayTag& InTerminalStateTag);

	// Keeps the current target unchanged when Candidate is ineligible.
	bool TrySetTarget(AActor* Candidate);

	// Idempotently releases the current target.
	void ClearTarget();

	bool HasTarget() const;

	// Returns null when no eligible target is currently owned.
	AActor* GetTarget() const;

	// Broadcasts after a successful target change with the previous and new actors.
	FTargetChangedEvent& OnTargetChanged()
	{
		return TargetChangedEvent;
	}

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction
	) override;

private:
	// The transition point for delegate ownership, state commit, and notification.
	void SetTarget(AActor* NewTarget);

	bool IsTargetEligible(const AActor* Candidate) const;

	UFUNCTION()
	void HandleTargetDestroyed(AActor* DestroyedActor);

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Targeting",
		meta = (ClampMin = "0.0", Units = "cm"))
	float MaximumTargetDistance = 2500.0f;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> CurrentTarget;

	FGameplayTag TerminalStateTag;
	FTargetChangedEvent TargetChangedEvent;
};
