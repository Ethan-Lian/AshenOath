#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "AshenOathHeavyChargeCostEffect.generated.h"

/** Instant SetByCaller Stamina change used by each heavy-charge drain tick. */
UCLASS()
class ASHENOATH_API UAshenOathHeavyChargeCostEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UAshenOathHeavyChargeCostEffect();
};
