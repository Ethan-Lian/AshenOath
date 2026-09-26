#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Ability/AshenOathMeleeAttackAbility.h"
#include "AshenOathBossComboAbility.generated.h"

class UAshenOathBossComboActionData;

/** One Boss attack execution owns every swing section in the combo. */
UCLASS()
class ASHENOATH_API UAshenOathBossComboAbility : public UAshenOathMeleeAttackAbility
{
	GENERATED_BODY()

public:
	UAshenOathBossComboAbility();

protected:
	virtual bool CanActivateAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags,
		const FGameplayTagContainer* TargetTags,
		FGameplayTagContainer* OptionalRelevantTags
	) const override;

	virtual void ActivateAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData
	) override;

private:
	bool IsComboActionDataReady(const UAshenOathBossComboActionData* ActionData) const;
};
