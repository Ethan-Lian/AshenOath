#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Ability/AshenOathCombatAbility.h"
#include "Defense/CombatDefenseTypes.h"
#include "AshenOathDodgeAbility.generated.h"

class UAbilityTask_ApplyCombatMovement;
class UAbilityTask_PlayMontageAndWait;
class UAshenOathAbilityTask_RecoverFacing;
class UAshenOathDodgeActionData;
class UCombatDefenseComponent;

enum class EAshenOathDodgeFacingMode : uint8
{
	PreserveCurrentFacing,
	FaceMovementDirection
};

/**
 * Coordinates one dodge execution across animation, defense, and movement tasks.
 */
UCLASS()
class ASHENOATH_API UAshenOathDodgeAbility : public UAshenOathCombatAbility
{
	GENERATED_BODY()

public:
	UAshenOathDodgeAbility();

	/** Returns true when GAS accepts activation with this frozen direction and facing policy. */
	bool TryActivateWithMovementDirection(
		const FVector& WorldDirection,
		EAshenOathDodgeFacingMode FacingMode
	);

#if WITH_DEV_AUTOMATION_TESTS
	// Simple automation tests advance a transient UWorld multiple times inside
	// one engine frame, so its GameplayTasks tick function runs only once.
	void TickTasksForTesting(float DeltaTime);
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
		const UAshenOathDodgeActionData* ActionData,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FVector& MovementDirection
	) const;

	UCombatDefenseComponent* ResolveDefenseComponent(
		const FGameplayAbilityActorInfo* ActorInfo
	) const;
	AActor* ResolveFacingTarget(const FGameplayAbilityActorInfo* ActorInfo) const;
	void ApplyActiveFacing(const FGameplayAbilityActorInfo* ActorInfo) const;

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

	UPROPERTY(Transient)
	TObjectPtr<UAshenOathAbilityTask_RecoverFacing> FacingRecoveryTask;

	TWeakObjectPtr<UCombatDefenseComponent> ActiveDefenseComponent;

	FCombatDefenseWindowHandle ActiveDefenseWindow;

	// Exists only while TryActivateAbility synchronously evaluates this request.
	FVector PendingMovementDirection = FVector::ZeroVector;
	EAshenOathDodgeFacingMode PendingFacingMode =
		EAshenOathDodgeFacingMode::PreserveCurrentFacing;

	// Frozen for the active execution and cleared by EndAbility.
	FVector ActiveMovementDirection = FVector::ZeroVector;
	EAshenOathDodgeFacingMode ActiveFacingMode =
		EAshenOathDodgeFacingMode::PreserveCurrentFacing;

	// Starts after travel; the Montage tail must leave this much time to recover.
	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Combat|Dodge",
		meta = (ClampMin = "0.01", Units = "s"))
	float FacingRecoveryDuration = 0.4f;
};
