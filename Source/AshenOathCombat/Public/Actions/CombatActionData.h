#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CombatActionData.generated.h"

class UGameplayEffect;
class UAnimMontage;

/**
 * Read-only configuration shared by every execution of an action.
 *
 * Runtime state belongs to the components executing this configuration rather
 * than this Data Asset. UCombatActionComponent owns the action and Montage;
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

	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category = "Combat Action|Cost")
	TSubclassOf<UGameplayEffect> CostEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Damage")
	TSubclassOf<UGameplayEffect> DamageEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Melee",meta = (ClampMin = "0.1", Units = "cm"))
	float MeleeTraceRadius = 12.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Melee")
	TArray<FName> MeleeTraceBones;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Damage")
	bool bCanTriggerPerfectDodge = true;
};
