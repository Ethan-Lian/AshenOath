#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Ability/AshenOathCombatAbility.h"
#include "Defense/CombatDefenseTypes.h"
#include "AshenOathDodgeAbility.generated.h"

class UAbilityTask_ApplyCombatMovement;
class UAbilityTask_PlayMontageAndWait;
class UCombatActionData;
class UCombatDefenseComponent;

/**
 * Coordinates one dodge execution across animation, defense, and movement tasks.
 */
UCLASS()
class ASHENOATH_API UAshenOathDodgeAbility : public UAshenOathCombatAbility
{
	GENERATED_BODY()

public:
	UAshenOathDodgeAbility();

	/** Attempts to activate this granted spec using the supplied world-space direction. */
	bool TryActivateWithMovementDirection(const FVector& WorldDirection);

#if WITH_DEV_AUTOMATION_TESTS
	// Simple automation tests advance a transient UWorld multiple times inside
	// one engine frame, so its GameplayTasks tick function runs only once.
	void TickMovementTaskForTesting(float DeltaTime);
#endif

protected:
	virtual bool CanActivateAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags,
		const FGameplayTagContainer* TargetTags,
		FGameplayTagContainer* OptionalRelevantTags
	) const override;

	virtual void ActivateAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData
	) override;

	virtual void EndAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled
	) override;

private:
	bool IsActionDataReady(
		const UCombatActionData* ActionData,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FVector& MovementDirection
	) const;

	UCombatDefenseComponent* ResolveDefenseComponent(
		const FGameplayAbilityActorInfo* ActorInfo
	) const;

	UFUNCTION()
	void HandleMontageCompleted();

	UFUNCTION()
	void HandleMontageInterrupted();

	UFUNCTION()
	void HandleMovementFailed();

	void FinishAbility(bool bWasCancelled);

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_ApplyCombatMovement> MovementTask;

	TWeakObjectPtr<UCombatDefenseComponent> ActiveDefenseComponent;

	FCombatDefenseWindowHandle ActiveDefenseWindow;

	// Exists only while TryActivateAbility synchronously evaluates this request.
	FVector PendingMovementDirection = FVector::ZeroVector;

	// Frozen for the active execution and cleared by EndAbility.
	FVector ActiveMovementDirection = FVector::ZeroVector;
};
