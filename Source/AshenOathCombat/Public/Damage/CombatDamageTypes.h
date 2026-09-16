#pragma once

#include "CoreMinimal.h"
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
};
