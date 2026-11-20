#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "CombatDamageTypes.generated.h"

class AActor;
class UGameplayEffect;

// Result of submitting one validated combat damage attempt.
UENUM(BlueprintType)
enum class ECombatDamageResult : uint8
{
	Applied,
	DodgeInvulnerable,
	OtherInvulnerable,
	Invalid
};

/**
 * Describes one source-authored damage request submitted to a target.
 * The receiving UCombatDamageComponent supplies the target; SourceActor and
 * HitTimeSeconds provide the source context and collision-time defense state.
 */
USTRUCT(BlueprintType)
struct ASHENOATHCOMBAT_API FCombatDamageAttempt
{
	GENERATED_BODY()

	// GameplayEffect applied when this hit is accepted.
	UPROPERTY(BlueprintReadWrite, Category = "Combat Damage")
	TSubclassOf<UGameplayEffect> DamageEffect;

	// Actor that initiated the attack and supplies the Source ASC.
	UPROPERTY(BlueprintReadWrite, Category = "Combat Damage")
	TObjectPtr<AActor> SourceActor = nullptr;

	// World time at which collision actually occurred.
	UPROPERTY(BlueprintReadWrite, Category = "Combat Damage")
	double HitTimeSeconds = 0.0;

	// Damage can be applied without interrupting the target or starting stagger.
	UPROPERTY(BlueprintReadWrite, Category = "Combat Damage")
	bool bCanTriggerHitReaction = true;

	// Optional per-execution SetByCaller value, applied only when its tag is valid.
	// The Effect decides which attribute consumes this value.
	UPROPERTY(BlueprintReadWrite, Category = "Combat Damage")
	FGameplayTag SetByCallerMagnitudeTag;

	UPROPERTY(BlueprintReadWrite, Category = "Combat Damage")
	float SetByCallerMagnitude = 0.0f;
};
