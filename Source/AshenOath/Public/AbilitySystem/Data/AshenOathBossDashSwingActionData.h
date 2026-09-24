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
		meta = (ClampMin = "0.0", Units = "cm"))
	float MaxDashDistance = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Boss Dash Swing",
		meta = (ClampMin = "0.0", Units = "cm"))
	float StopDistance = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Boss Dash Swing",
		meta = (ClampMin = "0.0", Units = "s"))
	float DashDurationSeconds = 0.0f;
};
