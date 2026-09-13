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
	RejectedMontageFailed,
	RejectedInvalidCost,
	RejectedInsufficientResources,
	RejectedCostApplicationFailed,
	RejectedBlockedByState
};

/**
 * Stable identity for one specific execution of a combat action.
 *
 * It identifies an action instance, not an action type. Each successful
 * execution receives a unique value, so callbacks and asynchronous events
 * can verify that they still belong to the currently active execution.
 *
 * Zero is reserved as the invalid identity.
 */
USTRUCT(BlueprintType)
struct ASHENOATHCOMBAT_API FCombatActionHandle
{
    GENERATED_BODY()

    // Unique identity of one combat action execution
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
