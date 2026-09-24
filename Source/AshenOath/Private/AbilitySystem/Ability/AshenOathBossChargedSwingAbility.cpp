#include "AbilitySystem/Ability/AshenOathBossChargedSwingAbility.h"

#include "AbilitySystem/Data/AshenOathBossChargedSwingActionData.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "GameplayTags/AshenOathGameplayTags.h"
#include "TimerManager.h"

UAshenOathBossChargedSwingAbility::UAshenOathBossChargedSwingAbility()
{
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(AshenOathGameplayTags::Ability_Action);
	AssetTags.AddTag(AshenOathGameplayTags::Ability_Action_BossChargedSwing);
	SetAssetTags(AssetTags);
}

bool UAshenOathBossChargedSwingAbility::CanActivateAbility(
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

	return IsChargedSwingDataReady(
		Cast<UAshenOathBossChargedSwingActionData>(
			ResolveActionData(Handle, ActorInfo)));
}

void UAshenOathBossChargedSwingAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	const UAshenOathBossChargedSwingActionData* ActionData =
		Cast<UAshenOathBossChargedSwingActionData>(ResolveActionData(Handle, ActorInfo));
	UWorld* World = GetWorld();
	if (!IsChargedSwingDataReady(ActionData) || !World)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ActiveActionData = ActionData;
	if (!StartAttackMontage(ActorInfo, ActionData, ActionData->WindupSection))
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

	AnimInstance->Montage_SetNextSection(
		ActionData->WindupSection, ActionData->ChargeLoopSection, Montage);
	AnimInstance->Montage_SetNextSection(
		ActionData->ChargeLoopSection, ActionData->ChargeLoopSection, Montage);

	ResetCostApplicationResult();
	const bool bCommitAccepted = CommitAbility(Handle, ActorInfo, ActivationInfo);
	if (!IsActive())
	{
		return;
	}
	if (!bCommitAccepted || !DidCostApplicationSucceed())
	{
		FinishAbility(true);
		return;
	}

	const int32 WindupIndex = Montage->GetSectionIndex(ActionData->WindupSection);
	const float WindupSeconds = Montage->GetSectionLength(WindupIndex) /
		ActionData->PlayRate;
	World->GetTimerManager().SetTimer(
		ChargeTimer,
		this,
		&UAshenOathBossChargedSwingAbility::BeginSwing,
		WindupSeconds + ActionData->ChargeDurationSeconds,
		false);
}

void UAshenOathBossChargedSwingAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChargeTimer);
	}
	ActiveActionData.Reset();

	Super::EndAbility(
		Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

bool UAshenOathBossChargedSwingAbility::IsChargedSwingDataReady(
	const UAshenOathBossChargedSwingActionData* ActionData) const
{
	if (!ActionData || !ActionData->Montage ||
		ActionData->WindupSection.IsNone() ||
		ActionData->ChargeLoopSection.IsNone() ||
		ActionData->SwingSection.IsNone() ||
		ActionData->WindupSection == ActionData->ChargeLoopSection ||
		ActionData->WindupSection == ActionData->SwingSection ||
		ActionData->ChargeLoopSection == ActionData->SwingSection ||
		ActionData->ChargeDurationSeconds <= KINDA_SMALL_NUMBER ||
		(!ActionData->StartSection.IsNone() &&
			ActionData->StartSection != ActionData->WindupSection))
	{
		return false;
	}

	const int32 WindupIndex =
		ActionData->Montage->GetSectionIndex(ActionData->WindupSection);
	return WindupIndex != INDEX_NONE &&
		ActionData->Montage->GetSectionLength(WindupIndex) > KINDA_SMALL_NUMBER &&
		ActionData->Montage->IsValidSectionName(ActionData->ChargeLoopSection) &&
		ActionData->Montage->IsValidSectionName(ActionData->SwingSection);
}

void UAshenOathBossChargedSwingAbility::BeginSwing()
{
	if (!IsActive())
	{
		return;
	}

	const UAshenOathBossChargedSwingActionData* ActionData = ActiveActionData.Get();
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UAnimInstance* AnimInstance = GetActiveAttackAnimInstance();
	UAnimMontage* Montage = GetActiveAttackMontage();
	if (!ActionData || !ActorInfo || !AnimInstance || !Montage)
	{
		FinishAbility(true);
		return;
	}

	if (!BeginMeleeExecution(
		GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(),
		ActionData, false))
	{
		return;
	}

	AnimInstance->Montage_JumpToSection(ActionData->SwingSection, Montage);
	AnimInstance->Montage_SetNextSection(ActionData->SwingSection, NAME_None, Montage);
}
