#pragma once

#include "CoreMinimal.h"

/**
 * Identifies one active melee detection session.
 *
 * The melee component allocates this identity. Callers keep the handle only
 * so stale window and cleanup requests cannot affect a newer session.
 */
struct ASHENOATHCOMBAT_API FCombatMeleeSessionHandle
{
	int32 Value = 0;

	bool IsValid() const
	{
		return Value != 0;
	}

	void Reset()
	{
		Value = 0;
	}
};
