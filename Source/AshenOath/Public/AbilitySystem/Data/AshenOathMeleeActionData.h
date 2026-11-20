#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Data/AshenOathActionData.h"
#include "AshenOathMeleeActionData.generated.h"

/** Project-authored damage and trace configuration for one melee ability. */
UCLASS(BlueprintType)
class ASHENOATH_API UAshenOathMeleeActionData : public UAshenOathActionData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Damage")
	TSubclassOf<UGameplayEffect> DamageEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Melee",
		meta = (ClampMin = "0.1", Units = "cm"))
	float MeleeTraceRadius = 12.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Melee")
	TArray<FName> MeleeTraceBones;
};
