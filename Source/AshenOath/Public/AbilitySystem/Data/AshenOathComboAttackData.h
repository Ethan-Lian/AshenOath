#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Data/AshenOathMeleeActionData.h"
#include "AshenOathComboAttackData.generated.h"

/** Player combo sections; runtime progress belongs to the executing Ability. */
UCLASS(BlueprintType)
class ASHENOATH_API UAshenOathComboAttackData : public UAshenOathMeleeActionData
{
	GENERATED_BODY()

public:
	// Empty preserves the single-section StartSection behavior.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Combo")
	TArray<FName> ComboSections;
};
