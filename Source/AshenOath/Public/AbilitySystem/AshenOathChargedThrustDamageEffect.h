#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "AshenOathChargedThrustDamageEffect.generated.h"

/** Instant SetByCaller Health change for the charged staff thrust. */
UCLASS()
class ASHENOATH_API UAshenOathChargedThrustDamageEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UAshenOathChargedThrustDamageEffect();
};
