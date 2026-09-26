#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Data/AshenOathActionData.h"
#include "AshenOathHealActionData.generated.h"

class UGameplayEffect;

/** Configuration for one interruptible player heal. */
UCLASS(BlueprintType)
class ASHENOATH_API UAshenOathHealActionData : public UAshenOathActionData
{
	GENERATED_BODY()

public:
	// Applied at the owned Montage's heal Notify, never when playback starts.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heal")
	TSubclassOf<UGameplayEffect> HealEffect;
};
