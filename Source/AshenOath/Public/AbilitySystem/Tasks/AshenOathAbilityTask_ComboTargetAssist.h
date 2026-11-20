#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AshenOathAbilityTask_ComboTargetAssist.generated.h"

class ACharacter;
class UAbilityTask_ApplyCombatMovement;
class UAnimInstance;
class UAshenOathComboAttackData;
class UCombatTargetingComponent;

/** First-hit approach and final-section facing, bounded by the authored hit windows. */
UCLASS()
class ASHENOATH_API UAshenOathAbilityTask_ComboTargetAssist : public UAbilityTask
{
	GENERATED_BODY()

public:
	UAshenOathAbilityTask_ComboTargetAssist(const FObjectInitializer& ObjectInitializer);

	static UAshenOathAbilityTask_ComboTargetAssist* AssistComboTarget(
		UGameplayAbility* OwningAbility,
		const UAshenOathComboAttackData* ActionData,
		int32 MontageInstanceId
	);

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;
	virtual void OnDestroy(bool bInOwnerFinished) override;

private:
	void BeginSectionAssist(int32 SectionIndex, float MontagePosition, float PlayRate);
	bool IsTargetEligible(const AActor* Target) const;
	void StopSectionAssist();
	void HandleTargetChanged(AActor* PreviousTarget, AActor* NewTarget);

	TWeakObjectPtr<ACharacter> Character;
	TWeakObjectPtr<UAnimInstance> AnimInstance;
	TWeakObjectPtr<UCombatTargetingComponent> Targeting;
	TWeakObjectPtr<const UAshenOathComboAttackData> Data;
	TWeakObjectPtr<AActor> AssistTarget;
	FDelegateHandle TargetChangedHandle;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_ApplyCombatMovement> MovementTask;

	int32 ActiveMontageInstanceId = INDEX_NONE;
	int32 CurrentSectionIndex = INDEX_NONE;
	float WindupEndPosition = 0.0f;
	bool bSectionAssistActive = false;
	bool bFirstSection = false;
};
