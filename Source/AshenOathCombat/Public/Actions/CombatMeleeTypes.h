#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class UAnimInstance;
class UAnimMontage;
class UGameplayEffect;
class USkeletalMeshComponent;

/** Input snapshot for one melee session. Source pointers are borrowed during BeginSession. */
struct ASHENOATHCOMBAT_API FCombatMeleeSessionRequest
{
	TSubclassOf<UGameplayEffect> DamageEffect;
	bool bCanTriggerPerfectDodge = false;
	float TraceRadius = 0.0f;
	TArray<FName> TraceBones;
	USkeletalMeshComponent* SourceMesh = nullptr;
	UAnimInstance* SourceAnimInstance = nullptr;
	UAnimMontage* SourceMontage = nullptr;
	int32 MontageInstanceId = INDEX_NONE;
	FGameplayTag SetByCallerMagnitudeTag;
	float SetByCallerMagnitude = 0.0f;
};

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
