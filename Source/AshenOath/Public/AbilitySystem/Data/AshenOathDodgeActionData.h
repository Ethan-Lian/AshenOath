#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Data/AshenOathActionData.h"
#include "GameplayTagContainer.h"
#include "AshenOathDodgeActionData.generated.h"

/** Project-authored movement and defense-window configuration for a dodge. */
UCLASS(BlueprintType)
class ASHENOATH_API UAshenOathDodgeActionData : public UAshenOathActionData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Movement",
		meta = (ClampMin = "0.0", Units = "cm"))
	float MovementDistance = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Movement",
		meta = (ClampMin = "0.0", Units = "s", EditCondition = "MovementDistance > 0.0", EditConditionHides))
	float MovementStartTime = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Movement",
		meta = (ClampMin = "0.01", Units = "s", EditCondition = "MovementDistance > 0.0", EditConditionHides))
	float MovementDuration = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Window")
	FGameplayTagContainer WindowTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Window",
		meta = (ClampMin = "0.0", Units = "s"))
	float WindowStartTime = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Window",
		meta = (ClampMin = "0.01", Units = "s"))
	float WindowDuration = 0.0f;

	// When enabled, this interval must be a proper subset of the ordinary window.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Window|Perfect Dodge",
		meta = (ClampMin = "0.0", Units = "s"))
	float PerfectDodgeWindowStartTime = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Window|Perfect Dodge",
		meta = (ClampMin = "0.0", Units = "s"))
	float PerfectDodgeWindowDuration = 0.0f;
};
