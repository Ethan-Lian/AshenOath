#pragma once

#include "CoreMinimal.h"
#include "CombatDefenseTypes.generated.h"

/** Stable identity for one defense window owned by UCombatDefenseComponent. */
USTRUCT(BlueprintType)
struct ASHENOATHCOMBAT_API FCombatDefenseWindowHandle
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Combat Defense")
	int32 Value = 0;

	bool IsValid() const
	{
		return Value != 0;
	}

	void Reset()
	{
		Value = 0;
	}
};
