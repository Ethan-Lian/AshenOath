#include "AbilitySystem/Ability/AshenOathComboAttackAbility.h"
#include "AbilitySystem/Data/AshenOathComboAttackData.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameplayTags/AshenOathGameplayTags.h"

UAshenOathComboAttackAbility::UAshenOathComboAttackAbility()
{
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(AshenOathGameplayTags::Ability_Action);
	AssetTags.AddTag(AshenOathGameplayTags::Ability_Action_ComboAttack);
	SetAssetTags(AssetTags);

	ActivationOwnedTags.AddTag(AshenOathGameplayTags::State_MovementLocked);
}

bool UAshenOathComboAttackAbility::TryQueueComboInput()
{
	if (!IsActive() || !bComboWindowOpen || bComboInputConsumedInWindow ||
		ActiveComboWindowNotifyInstanceId == INDEX_NONE)
	{
		return false;
	}

	UAnimMontage* Montage = GetActiveAttackMontage();
	UAnimInstance* AnimInstance = GetActiveAttackAnimInstance();
	if (!AnimInstance || !Montage)
	{
		return false;
	}

	const FAnimMontageInstance* MontageInstance =
		AnimInstance->GetActiveInstanceForMontage(Montage);
	if (!MontageInstance ||
		MontageInstance->GetInstanceID() != GetActiveAttackMontageInstanceId())
	{
		return false;
	}

	const FName CurrentSection = AnimInstance->Montage_GetCurrentSection(Montage);
	const int32 CurrentSectionIndex = ActiveComboSections.IndexOfByKey(CurrentSection);
	const int32 NextSectionIndex = CurrentSectionIndex + 1;

	if (CurrentSectionIndex == INDEX_NONE ||
		!ActiveComboSections.IsValidIndex(NextSectionIndex))
	{
		return false;
	}

	AnimInstance->Montage_SetNextSection(
		CurrentSection,
		ActiveComboSections[NextSectionIndex],
		Montage
	);

	bComboInputConsumedInWindow = true;
	return true;
}

void UAshenOathComboAttackAbility::BeginComboWindowFromAnimation(
	USkeletalMeshComponent* MeshComponent,
	UAnimSequenceBase* Animation,
	const int32 MontageInstanceId,
	const int32 NotifyInstanceId)
{
	if (!IsAttackSignalOwned(MeshComponent, Animation, MontageInstanceId) ||
		NotifyInstanceId == INDEX_NONE || bComboWindowOpen)
	{
		return;
	}

	UAnimInstance* AnimInstance = GetActiveAttackAnimInstance();
	UAnimMontage* Montage = GetActiveAttackMontage();
	if (!AnimInstance || !Montage)
	{
		return;
	}

	const FName CurrentSection = AnimInstance->Montage_GetCurrentSection(Montage);
	const int32 CurrentSectionIndex = ActiveComboSections.IndexOfByKey(CurrentSection);
	if (CurrentSectionIndex == INDEX_NONE ||
		!ActiveComboSections.IsValidIndex(CurrentSectionIndex + 1))
	{
		return;
	}

	ActiveComboWindowNotifyInstanceId = NotifyInstanceId;
	bComboWindowOpen = true;
	bComboInputConsumedInWindow = false;
}

void UAshenOathComboAttackAbility::EndComboWindowFromAnimation(
	USkeletalMeshComponent* MeshComponent,
	UAnimSequenceBase* Animation,
	const int32 MontageInstanceId,
	const int32 NotifyInstanceId)
{
	if (!IsAttackSignalOwned(MeshComponent, Animation, MontageInstanceId) ||
		!bComboWindowOpen ||
		NotifyInstanceId != ActiveComboWindowNotifyInstanceId)
	{
		return;
	}

	ActiveComboWindowNotifyInstanceId = INDEX_NONE;
	bComboWindowOpen = false;
	bComboInputConsumedInWindow = false;
}

bool UAshenOathComboAttackAbility::CanActivateAbility(
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

	return IsComboActionDataReady(
		Cast<UAshenOathComboAttackData>(ResolveActionData(Handle, ActorInfo)));
}

void UAshenOathComboAttackAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	const UAshenOathComboAttackData* ActionData =
		Cast<UAshenOathComboAttackData>(ResolveActionData(Handle, ActorInfo));
	if (!IsComboActionDataReady(ActionData))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FName InitialSection = !ActionData->ComboSections.IsEmpty()
		? ActionData->ComboSections[0]
		: ActionData->StartSection;

	StartComboExecution(
		Handle,
		ActorInfo,
		ActivationInfo,
		ActionData,
		InitialSection
	);
}

void UAshenOathComboAttackAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	ResetComboState();
	Super::EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		bReplicateEndAbility,
		bWasCancelled
	);
}

bool UAshenOathComboAttackAbility::IsComboActionDataReady(
	const UAshenOathComboAttackData* ActionData) const
{
	if (!ActionData || !ActionData->Montage)
	{
		return false;
	}

	if (ActionData->ComboSections.IsEmpty())
	{
		return ActionData->StartSection.IsNone() ||
			ActionData->Montage->IsValidSectionName(ActionData->StartSection);
	}

	for (const FName SectionName : ActionData->ComboSections)
	{
		if (SectionName.IsNone() ||
			!ActionData->Montage->IsValidSectionName(SectionName))
		{
			return false;
		}
	}

	return true;
}

bool UAshenOathComboAttackAbility::StartComboExecution(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const UAshenOathComboAttackData* ActionData,
	const FName InitialSection)
{
	ActiveComboSections = ActionData->ComboSections;
	if (!StartAttackMontage(ActorInfo, ActionData, InitialSection))
	{
		return false;
	}

	UAnimInstance* AnimInstance = GetActiveAttackAnimInstance();
	UAnimMontage* Montage = GetActiveAttackMontage();
	if (!AnimInstance || !Montage)
	{
		FinishAbility(true);
		return false;
	}

	// Continuation is opt-in. Asset links cannot advance the chain unless a
	// matching Combo Window consumes a repeated combo-attack input.
	for (const FName SectionName : ActiveComboSections)
	{
		AnimInstance->Montage_SetNextSection(SectionName, NAME_None, Montage);
	}

	return BeginMeleeExecution(
		Handle,
		ActorInfo,
		ActivationInfo,
		ActionData,
		true
	);
}

void UAshenOathComboAttackAbility::ResetComboState()
{
	ActiveComboSections.Reset();
	ActiveComboWindowNotifyInstanceId = INDEX_NONE;
	bComboWindowOpen = false;
	bComboInputConsumedInWindow = false;
}
