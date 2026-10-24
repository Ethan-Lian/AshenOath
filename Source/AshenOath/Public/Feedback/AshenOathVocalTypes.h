#pragma once

#include "CoreMinimal.h"
#include "AshenOathVocalTypes.generated.h"

UENUM(BlueprintType)
enum class EAshenOathVocalType : uint8
{
	LightAttack,
	ComboFinisher,
	HeavyAttack,
	SpecialAttack,
	Dodge,
	HeavyComboFinisher
};

USTRUCT(BlueprintType)
struct FAshenOathVocalRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vocal", meta = (ClampMin = "0", ClampMax = "1"))
	float Chance = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vocal", meta = (ClampMin = "0"))
	float Volume = 0.56f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vocal", meta = (ClampMin = "0", ClampMax = "0.1"))
	float PitchVariation = 0.025f;
};
