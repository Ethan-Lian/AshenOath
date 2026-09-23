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

	// The target Defense component still decides timing and one-time consumption.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Damage")
	bool bCanTriggerPerfectDodge = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Melee",
		meta = (ClampMin = "0.1", Units = "cm"))
	float MeleeTraceRadius = 12.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Melee")
	TArray<FName> MeleeTraceBones;
};
