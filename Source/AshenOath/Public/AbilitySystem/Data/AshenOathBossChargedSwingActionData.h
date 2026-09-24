#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Data/AshenOathMeleeActionData.h"
#include "AshenOathBossChargedSwingActionData.generated.h"

/** Montage stages and timing for one Boss charged swing. */
UCLASS(BlueprintType)
class ASHENOATH_API UAshenOathBossChargedSwingActionData : public UAshenOathMeleeActionData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Boss Charged Swing")
	FName WindupSection = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Boss Charged Swing")
	FName ChargeLoopSection = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Boss Charged Swing")
	FName SwingSection = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Boss Charged Swing",
		meta = (ClampMin = "0.0", Units = "s"))
	float ChargeDurationSeconds = 0.0f;
};
