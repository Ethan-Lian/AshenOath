#include "AbilitySystem/Ability/AshenOathBossSingleSwingAbility.h"

#include "AbilitySystem/Data/AshenOathMeleeActionData.h"
#include "Animation/AnimMontage.h"
#include "GameplayTags/AshenOathGameplayTags.h"

UAshenOathBossSingleSwingAbility::UAshenOathBossSingleSwingAbility()
{
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(AshenOathGameplayTags::Ability_Action);
	AssetTags.AddTag(AshenOathGameplayTags::Ability_Action_BossSingleSwing);
	SetAssetTags(AssetTags);
}

bool UAshenOathBossSingleSwingAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(
		Handle,
		ActorInfo,
		SourceTags,
		TargetTags,
		OptionalRelevantTags))
	{
		return false;
	}

	return IsActionDataReady(
		Cast<UAshenOathMeleeActionData>(ResolveActionData(Handle, ActorInfo))
	);
}

void UAshenOathBossSingleSwingAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	const UAshenOathMeleeActionData* ActionData =
		Cast<UAshenOathMeleeActionData>(ResolveActionData(Handle, ActorInfo));
	if (!IsActionDataReady(ActionData))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!StartAttackMontage(ActorInfo, ActionData, ActionData->StartSection))
	{
		return;
	}

	BeginMeleeExecution(Handle, ActorInfo, ActivationInfo, ActionData, true);
}

bool UAshenOathBossSingleSwingAbility::IsActionDataReady(
	const UAshenOathMeleeActionData* ActionData) const
{
	return ActionData && ActionData->Montage &&
		(ActionData->StartSection.IsNone() ||
			ActionData->Montage->IsValidSectionName(ActionData->StartSection));
}
