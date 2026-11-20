#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Ability/AshenOathCombatAbility.h"
#include "Defense/CombatDefenseTypes.h"
#include "AshenOathDodgeAbility.generated.h"

class UAbilityTask_ApplyCombatMovement;
class UAbilityTask_PlayMontageAndWait;
class UAshenOathAbilityTask_RecoverFacing;
class UAshenOathDodgeActionData;
class UAnimMontage;
class UCombatDefenseComponent;
class UAshenOathCombatFeedbackComponent;
class USkeletalMeshComponent;
class UAnimSequenceBase;
class USoundBase;

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
	void RequestVocalFromAnimation(
		USkeletalMeshComponent* MeshComponent, UAnimSequenceBase* Animation, int32 MontageInstanceId, USoundBase* Sound);

	/** Returns true when GAS accepts activation with this frozen direction and facing policy. */
	bool TryActivateWithMovementDirection(
		const FVector& WorldDirection,
		EAshenOathDodgeFacingMode FacingMode
	);

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
	enum class EFinishReason : uint8
	{
		Completed,
		Cancelled
	};

	bool IsActionDataReady(
		const UAshenOathDodgeActionData* ActionData,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FVector& MovementDirection
	) const;
	bool StartDodgeMontage(
		const FGameplayAbilityActorInfo* ActorInfo,
		const UAshenOathDodgeActionData* ActionData
	);
	bool IsDodgeMontageOwned(
		const FGameplayAbilityActorInfo* ActorInfo,
		const UAnimMontage* Montage
	);
	bool CommitConfiguredCost(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo
	);
	bool StartDefenseWindow(
		const FGameplayAbilityActorInfo* ActorInfo,
		const UAshenOathDodgeActionData* ActionData,
		double& OutExecutionStartWorldTime
	);
	bool StartFacingRecoveryIfNeeded(
		const FGameplayAbilityActorInfo* ActorInfo,
		const UAshenOathDodgeActionData* ActionData,
		double ExecutionStartWorldTime
	);
	bool StartMovementTask(
		const UAshenOathDodgeActionData* ActionData,
		double ExecutionStartWorldTime
	);

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

	void FinishAbility(EFinishReason Reason);

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_ApplyCombatMovement> MovementTask;

	UPROPERTY(Transient)
	TObjectPtr<UAshenOathAbilityTask_RecoverFacing> FacingRecoveryTask;

	TWeakObjectPtr<UCombatDefenseComponent> ActiveDefenseComponent;
	TWeakObjectPtr<UAshenOathCombatFeedbackComponent> ActiveFeedbackComponent;
	TWeakObjectPtr<UAnimMontage> ActiveVocalMontage;
	int32 ActiveVocalMontageInstanceId = INDEX_NONE;
	uint32 ActiveVocalActionId = 0;

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
