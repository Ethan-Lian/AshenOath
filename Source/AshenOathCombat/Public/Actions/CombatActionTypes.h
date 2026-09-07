#pragma once
#include "CoreMinimal.h"
#include "CombatActionTypes.generated.h"

/**
 * Describes whether an action request started or why it was rejected.
 */

UENUM(BlueprintType)
enum class ECombatActionStartResult : uint8
{
	Started,
	RejectedAlreadyActive,
	RejectedInvalidOwner,
	RejectedInvalidAnimation,
	RejectedInvalidData,
	RejectedMontageFailed
};

/**
 * Identifies one execution of an action rather than an action type.
 *
 * Each successful execution receives a different value, allowing delayed
 * callbacks from an older execution to be rejected safely.
 * Zero is reserved as the invalid value.
 */
USTRUCT(BlueprintType)
struct ASHENOATHCOMBAT_API FCombatActionHandle
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Combat Action")
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
