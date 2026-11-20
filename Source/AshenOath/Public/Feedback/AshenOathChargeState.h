#pragma once

#include "CoreMinimal.h"

/** Read-only presentation of the charge owned by HeavyAttackAbility. */
struct FAshenOathChargeState
{
	bool bCharging = false;
	bool bCanRelease = false;
	float ChargeAlpha = 0.0f;
};
