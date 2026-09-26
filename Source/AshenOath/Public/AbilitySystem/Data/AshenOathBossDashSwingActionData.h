#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Data/AshenOathMeleeActionData.h"
#include "AshenOathBossDashSwingActionData.generated.h"

/** Dash and swing configuration; the Ability owns their execution handoff. */
UCLASS(BlueprintType)
class ASHENOATH_API UAshenOathBossDashSwingActionData : public UAshenOathMeleeActionData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Boss Dash Swing")
	FName DashSection = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Boss Dash Swing")
	FName SwingSection = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Boss Dash Swing",
		meta = (ClampMin = "0.01", Units = "cm"))
	float StopDistance = 300.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Boss Dash Swing",
		meta = (ClampMin = "1.01"))
	float DashSpeedMultiplier = 2.0f;
};
