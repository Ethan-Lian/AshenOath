#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Data/AshenOathMeleeActionData.h"
#include "AshenOathBossComboActionData.generated.h"

/** Section configuration for one Boss combo; execution progress stays in its Ability. */
UCLASS(BlueprintType)
class ASHENOATH_API UAshenOathBossComboActionData : public UAshenOathMeleeActionData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Boss Combo")
	TArray<FName> SwingSections;
};
