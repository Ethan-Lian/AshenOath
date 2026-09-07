#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CombatActionData.generated.h"

class UAnimMontage;

/**
 * Read-only configuration shared by every execution of an action.
 *
 * Runtime state such as the active Montage and execution handle belongs to
 * UCombatActionComponent rather than this Data Asset.
 */

UCLASS(BlueprintType)
class ASHENOATHCOMBAT_API UCombatActionData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Combat Action"
	)
	TObjectPtr<UAnimMontage> Montage;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Combat Action",
		meta = (ClampMin = "0.01")
	)
	float PlayRate = 1.0f;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "Combat Action"
	)
	FName StartSection = NAME_None;
};