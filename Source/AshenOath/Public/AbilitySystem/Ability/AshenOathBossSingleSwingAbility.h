#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Ability/AshenOathMeleeAttackAbility.h"
#include "AshenOathBossSingleSwingAbility.generated.h"

class UAshenOathMeleeActionData;

/** Boss single-swing activation; the melee base owns execution and cleanup. */
UCLASS()
class ASHENOATH_API UAshenOathBossSingleSwingAbility : public UAshenOathMeleeAttackAbility
{
	GENERATED_BODY()

public:
	UAshenOathBossSingleSwingAbility();

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
	bool IsActionDataReady(const UAshenOathMeleeActionData* ActionData) const;
};
