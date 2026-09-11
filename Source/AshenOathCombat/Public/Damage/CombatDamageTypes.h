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
	Duplicate,
	Invalid
};

/**
 * Describes one attempted hit independently of its collision source.
 *
 * The damage receiver is the owner of UCombatDamageComponent.
 * SourceActor supplies the outgoing ASC and gameplay effect context.
 *
 * A hit is uniquely identified by:
 * SourceActor + AttackInstanceId + HitId + DamageEffect.
 */
USTRUCT(BlueprintType)
struct ASHENOATHCOMBAT_API FCombatDamageAttempt
{
	GENERATED_BODY()

	// GameplayEffect applied when this hit is accepted.
	UPROPERTY(BlueprintReadWrite, Category = "Combat Damage")
	TSubclassOf<UGameplayEffect> DamageEffect;

	// Actor that initiated the attack and supplies the outgoing ASC.
	UPROPERTY(BlueprintReadWrite, Category = "Combat Damage")
	TObjectPtr<AActor> SourceActor = nullptr;

	// Identifies one runtime attack window instance.
	// Prevents overlapping or repeated montage/notifies from sharing hit state.
	UPROPERTY(BlueprintReadWrite, Category = "Combat Damage")
	int32 HitId = 0;

	// Identifies one runtime attack window instance.
	// Prevents overlapping or repeated montage/notifies from sharing hit state.
	UPROPERTY(BlueprintReadWrite, Category = "Combat Damage")
	int32 AttackInstanceId = 0;

	// World time at which collision actually occurred.
	UPROPERTY(BlueprintReadWrite, Category = "Combat Damage")
	double HitTimeSeconds = 0.0;

	UPROPERTY(BlueprintReadWrite, Category = "Combat Damage")
	bool bCanTriggerPerfectDodge = false;
};
