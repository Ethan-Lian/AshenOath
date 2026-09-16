#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "AshenOathStaminaRegenerationEffect.generated.h"

// Default player stamina regeneration. StaminaRecovery owns when this effect
// is active; the effect itself keeps all numeric mutation inside GAS.
UCLASS()
class ASHENOATH_API UAshenOathStaminaRegenerationEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UAshenOathStaminaRegenerationEffect();
};
