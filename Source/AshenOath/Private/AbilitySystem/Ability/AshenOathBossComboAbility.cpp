#include "AbilitySystem/Ability/AshenOathBossComboAbility.h"

#include "AbilitySystem/Data/AshenOathBossComboActionData.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "GameplayTags/AshenOathGameplayTags.h"

UAshenOathBossComboAbility::UAshenOathBossComboAbility()
{
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(AshenOathGameplayTags::Ability_Action);
	AssetTags.AddTag(AshenOathGameplayTags::Ability_Action_BossCombo);
	SetAssetTags(AssetTags);
}

bool UAshenOathBossComboAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(
		Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	return IsComboActionDataReady(
		Cast<UAshenOathBossComboActionData>(ResolveActionData(Handle, ActorInfo)));
}

void UAshenOathBossComboAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	const UAshenOathBossComboActionData* ActionData =
		Cast<UAshenOathBossComboActionData>(ResolveActionData(Handle, ActorInfo));
	if (!IsComboActionDataReady(ActionData))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!StartAttackMontage(ActorInfo, ActionData, ActionData->SwingSections[0]))
	{
		return;
	}

	UAnimInstance* AnimInstance = GetActiveAttackAnimInstance();
	UAnimMontage* Montage = GetActiveAttackMontage();
	if (!AnimInstance || !Montage)
	{
		FinishAbility(true);
		return;
	}

	// Explicit links prevent unrelated asset sections from entering this combo.
	for (int32 Index = 0; Index < ActionData->SwingSections.Num(); ++Index)
	{
		const FName NextSection = ActionData->SwingSections.IsValidIndex(Index + 1)
			? ActionData->SwingSections[Index + 1]
			: NAME_None;
		AnimInstance->Montage_SetNextSection(
			ActionData->SwingSections[Index], NextSection, Montage);
	}

	BeginMeleeExecution(Handle, ActorInfo, ActivationInfo, ActionData, true);
}

bool UAshenOathBossComboAbility::IsComboActionDataReady(
	const UAshenOathBossComboActionData* ActionData) const
{
	if (!ActionData || !ActionData->Montage ||
		ActionData->SwingSections.Num() < 2 ||
		(!ActionData->StartSection.IsNone() &&
			ActionData->StartSection != ActionData->SwingSections[0]))
	{
		return false;
	}

	TSet<FName> SeenSections;
	for (const FName Section : ActionData->SwingSections)
	{
		if (Section.IsNone() ||
			!ActionData->Montage->IsValidSectionName(Section) ||
			SeenSections.Contains(Section))
		{
			return false;
		}

		SeenSections.Add(Section);
	}

	return true;
}
