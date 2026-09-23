#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Data/AshenOathMeleeActionData.h"
#include "AshenOathHeavyAttackData.generated.h"

/** Player heavy attack configuration; runtime press/hold state stays in the Ability. */
UCLASS(BlueprintType)
class ASHENOATH_API UAshenOathHeavyAttackData : public UAshenOathMeleeActionData
{
	GENERATED_BODY()

public:
	UAshenOathHeavyAttackData();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heavy Attack|Charge",
		meta = (ClampMin = "0.01", Units = "s"))
	float MaxChargeDurationSeconds = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heavy Attack|Charge",
		meta = (ClampMin = "0.0"))
	float MaxChargeStaminaCost = 150.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heavy Attack|Charge",
		meta = (ClampMin = "0.01", Units = "s"))
	float ChargeDrainIntervalSeconds = 0.1f;

	// Must be an Instant Effect whose Stamina modifier reads Data.Cost.Stamina.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heavy Attack|Charge")
	TSubclassOf<UGameplayEffect> ChargeCostEffect;

	// Keep this equal to the configured normal-attack damage magnitude.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heavy Attack|Damage",
		meta = (ClampMin = "0.0"))
	float NormalAttackDamageMagnitude = 10.0f;

	// Damage grows linearly from the normal-attack baseline to this multiplier.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heavy Attack|Damage",
		meta = (ClampMin = "1.0"))
	float FullyChargedDamageMultiplier = 6.0f;

	// At release, the active Boss is eligible for one horizontal facing correction
	// only while it remains within this angle from the current facing direction.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heavy Attack|Release Aim",
		meta = (ClampMin = "0.0", ClampMax = "180.0", Units = "deg"))
	float ReleaseAutoAimMaxAngleDegrees = 60.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heavy Attack|Release Aim",
		meta = (ClampMin = "0.0", Units = "cm"))
	float ReleaseAutoAimMaxDistance = 600.0f;

	// Plays once before the linked charge loop.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heavy Attack|Charge")
	FName ChargeStartSection = NAME_None;

	// Loops until the ability jumps to ChargedReleaseSection.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heavy Attack|Charge")
	FName ChargeLoopSection = NAME_None;

	// Must contain the melee hit-window notifies used by the release attack.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heavy Attack|Charge")
	FName ChargedReleaseSection = NAME_None;
};
