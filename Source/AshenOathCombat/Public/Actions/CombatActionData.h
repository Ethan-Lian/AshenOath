#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "CombatActionData.generated.h"

class UGameplayEffect;
class UAnimMontage;

/**
 * Read-only configuration shared by every execution of an action.
 *
 * Runtime state belongs to the objects executing this configuration rather than
 * this Data Asset. GameplayAbility normally owns the action and Montage task;
 * UCombatMeleeComponent owns the active melee snapshot and hit-window state.
 */

UCLASS(BlueprintType)
class ASHENOATHCOMBAT_API UCombatActionData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action")
	TObjectPtr<UAnimMontage> Montage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action", meta = (ClampMin = "0.01"))
	float PlayRate = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action")
	FName StartSection = NAME_None;

	// Optional instant GameplayEffect committed only after the Montage starts.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Cost")
	TSubclassOf<UGameplayEffect> CostEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Damage")
	TSubclassOf<UGameplayEffect> DamageEffect;

	// Source-authored eligibility only. The target Defense component still
	// decides timing and enforces one perfect-dodge result per dodge action.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Damage")
	bool bCanTriggerPerfectDodge = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Melee",
		meta = (ClampMin = "0.1", Units = "cm"))
	float MeleeTraceRadius = 12.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Melee")
	TArray<FName> MeleeTraceBones;

	// Optional code-driven displacement. A zero distance disables movement for
	// actions such as stationary attacks.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Movement",
		meta = (ClampMin = "0.0", Units = "cm"))
	float MovementDistance = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Movement",
		meta = (ClampMin = "0.0", Units = "s", EditCondition = "MovementDistance > 0.0", EditConditionHides))
	float MovementStartTime = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Movement",
		meta = (ClampMin = "0.01", Units = "s", EditCondition = "MovementDistance > 0.0", EditConditionHides))
	float MovementDuration = 0.0f;

	// Tags granted only during this action-relative window.
	// Dodge data uses State.Invulnerable.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Window")
	FGameplayTagContainer WindowTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Window",
		meta = (ClampMin = "0.0", Units = "s"))
	float WindowStartTime = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Window",
		meta = (ClampMin = "0.01", Units = "s"))
	float WindowDuration = 0.0f;

	// Dodge-only timing. When enabled by the Dodge Ability this interval must
	// be a proper subset of the ordinary defensive window above.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Window|Perfect Dodge",
		meta = (ClampMin = "0.0", Units = "s"))
	float PerfectDodgeWindowStartTime = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Window|Perfect Dodge",
		meta = (ClampMin = "0.0", Units = "s"))
	float PerfectDodgeWindowDuration = 0.0f;
};
