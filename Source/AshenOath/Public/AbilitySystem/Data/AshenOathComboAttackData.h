#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Data/AshenOathMeleeActionData.h"
#include "AshenOathComboAttackData.generated.h"

/** Player combo sections; runtime progress belongs to the executing Ability. */
UCLASS(BlueprintType)
class ASHENOATH_API UAshenOathComboAttackData : public UAshenOathMeleeActionData
{
	GENERATED_BODY()

public:
	// Empty preserves the single-section StartSection behavior.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Combo")
	TArray<FName> ComboSections;

	// Assistance ends at the first Combat Hit Window in the section.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Target Assist",
		meta = (ClampMin = "0.0", Units = "cm"))
	float AssistMaxDistance = 550.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Target Assist",
		meta = (ClampMin = "0.0", ClampMax = "180.0", Units = "deg"))
	float AssistMaxAngleDegrees = 120.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Target Assist",
		meta = (ClampMin = "0.0", Units = "cm"))
	float AssistMaxHeightDifference = 150.0f;

	// Maximum first-hit travel, toward the target position sampled at startup.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Target Assist|Approach",
		meta = (ClampMin = "0.0", Units = "cm"))
	float ApproachMaxTravelDistance = 350.0f;

	// Clearance between the characters' capsules, rather than actor centers.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Target Assist|Approach",
		meta = (ClampMin = "0.0", Units = "cm"))
	float ApproachStopGap = 65.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Target Assist|Approach",
		meta = (ClampMin = "0.01", Units = "s"))
	float ApproachDuration = 0.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Target Assist|Finisher",
		meta = (ClampMin = "0.0", Units = "deg/s"))
	float FinisherTurnRate = 720.0f;
};
