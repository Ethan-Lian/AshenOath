#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AshenOathActionData.generated.h"

class UAnimMontage;
class UGameplayEffect;

/** Shared configuration read by the project's GameplayAbilities. */
UCLASS(BlueprintType)
class ASHENOATH_API UAshenOathActionData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action")
	TObjectPtr<UAnimMontage> Montage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action", meta = (ClampMin = "0.01"))
	float PlayRate = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action")
	FName StartSection = NAME_None;

	// Optional instant GameplayEffect committed only after the Montage starts.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action|Cost")
	TSubclassOf<UGameplayEffect> CostEffect;
};
