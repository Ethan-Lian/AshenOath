#include "AbilitySystem/Ability/AshenOathHealAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystem/Data/AshenOathHealActionData.h"
#include "AbilitySystemComponent.h"
#include "ActiveGameplayEffectHandle.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Characters/AshenOathPlayerCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameplayEffect.h"
#include "GameplayTags/AshenOathGameplayTags.h"

UAshenOathHealAbility::UAshenOathHealAbility()
{
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(AshenOathGameplayTags::Ability_Action);
	AssetTags.AddTag(AshenOathGameplayTags::Ability_Action_Heal);
	SetAssetTags(AssetTags);
	ActivationOwnedTags.AddTag(AshenOathGameplayTags::State_MovementLocked);
}

bool UAshenOathHealAbility::CanActivateAbility(
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

	const AAshenOathPlayerCharacter* Player = ActorInfo
		? Cast<AAshenOathPlayerCharacter>(ActorInfo->AvatarActor.Get())
		: nullptr;

	return Player && Player->CanStartHeal() && ActorInfo->GetAnimInstance() &&
		ActorInfo->SkeletalMeshComponent.IsValid() &&
		IsHealActionDataReady(Cast<UAshenOathHealActionData>(
			ResolveActionData(Handle, ActorInfo)));
}

void UAshenOathHealAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	const UAshenOathHealActionData* ActionData =
		Cast<UAshenOathHealActionData>(ResolveActionData(Handle, ActorInfo));
	AAshenOathPlayerCharacter* Player = ActorInfo
		? Cast<AAshenOathPlayerCharacter>(ActorInfo->AvatarActor.Get())
		: nullptr;

	if (!Player || !Player->CanStartHeal() || !IsHealActionDataReady(ActionData))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ActivePlayer = Player;
	ActiveHealData = ActionData;

	UAbilityTask_PlayMontageAndWait* NewMontageTask =
		UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this,
			TEXT("PlayerHealMontage"),
			ActionData->Montage,
			ActionData->PlayRate,
			ActionData->StartSection,
			true,
			1.0f,
			0.0f,
			true
		);

	if (!NewMontageTask)
	{
		FinishAbility(true);
		return;
	}

	MontageTask = NewMontageTask;
	NewMontageTask->OnCompleted.AddDynamic(this, &UAshenOathHealAbility::HandleMontageCompleted);
	NewMontageTask->OnInterrupted.AddDynamic(this, &UAshenOathHealAbility::HandleMontageInterrupted);
	NewMontageTask->OnCancelled.AddDynamic(this, &UAshenOathHealAbility::HandleMontageInterrupted);
	NewMontageTask->ReadyForActivation();

	if (!IsActive())
	{
		return;
	}

	UAbilitySystemComponent* AbilitySystem = ActorInfo
		? ActorInfo->AbilitySystemComponent.Get()
		: nullptr;
	UAnimInstance* AnimInstance = ActorInfo ? ActorInfo->GetAnimInstance() : nullptr;
	USkeletalMeshComponent* MeshComponent = ActorInfo
		? ActorInfo->SkeletalMeshComponent.Get()
		: nullptr;
	const FAnimMontageInstance* MontageInstance = AnimInstance
		? AnimInstance->GetActiveInstanceForMontage(ActionData->Montage)
		: nullptr;

	if (!AbilitySystem || !AbilitySystem->IsAnimatingAbility(this) ||
		AbilitySystem->GetCurrentMontage() != ActionData->Montage ||
		!MeshComponent || !MontageInstance)
	{
		FinishAbility(true);
		return;
	}

	ActiveSourceMesh = MeshComponent;
	ActiveSourceAnimInstance = AnimInstance;
	ActiveSourceMontage = ActionData->Montage;
	ActiveMontageInstanceId = MontageInstance->GetInstanceID();
}

bool UAshenOathHealAbility::TryCommitHealFromAnimation(
	USkeletalMeshComponent* MeshComponent,
	UAnimSequenceBase* Animation,
	const int32 MontageInstanceId)
{
	if (bHealCommitAttempted ||
		!IsHealSignalOwned(MeshComponent, Animation, MontageInstanceId))
	{
		return false;
	}

	AAshenOathPlayerCharacter* Player = ActivePlayer.Get();
	if (!IsValid(Player) || !Player->CanStartHeal())
	{
		return false;
	}

	// Claim this Notify before any call that could trigger Gameplay callbacks.
	bHealCommitAttempted = true;

	if (!Player->TryReserveHealUse())
	{
		return false;
	}

	const bool bEffectApplied = ApplyConfiguredHealEffect();

	// Effect application may synchronously cancel this Ability. Use the
	// captured player instead of reading ActivePlayer again afterward.
	if (IsValid(Player))
	{
		Player->ResolveHealUseReservation(bEffectApplied);
	}

	if (!bEffectApplied && IsActive())
	{
		FinishAbility(true);
	}

	return bEffectApplied;
}

void UAshenOathHealAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	MontageTask = nullptr;
	ActivePlayer.Reset();
	ActiveHealData.Reset();
	ActiveSourceMesh.Reset();
	ActiveSourceAnimInstance.Reset();
	ActiveSourceMontage.Reset();
	ActiveMontageInstanceId = INDEX_NONE;
	bHealCommitAttempted = false;

	Super::EndAbility(
		Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

bool UAshenOathHealAbility::IsHealActionDataReady(
	const UAshenOathHealActionData* ActionData) const
{
	const UGameplayEffect* HealEffect = ActionData && ActionData->HealEffect
		? ActionData->HealEffect.GetDefaultObject()
		: nullptr;

	return ActionData && ActionData->Montage &&
		ActionData->PlayRate > KINDA_SMALL_NUMBER &&
		(ActionData->StartSection.IsNone() ||
			ActionData->Montage->IsValidSectionName(ActionData->StartSection)) &&
		!ActionData->CostEffect && HealEffect &&
		HealEffect->DurationPolicy == EGameplayEffectDurationType::Instant;
}

bool UAshenOathHealAbility::IsHealSignalOwned(
	const USkeletalMeshComponent* MeshComponent,
	const UAnimSequenceBase* Animation,
	const int32 MontageInstanceId) const
{
	const UAnimInstance* AnimInstance = ActiveSourceAnimInstance.Get();
	const UAnimMontage* Montage = ActiveSourceMontage.Get();
	const FAnimMontageInstance* MontageInstance = AnimInstance && Montage
		? AnimInstance->GetActiveInstanceForMontage(Montage)
		: nullptr;

	return IsActive() && MeshComponent &&
		MeshComponent == ActiveSourceMesh.Get() &&
		MeshComponent->GetAnimInstance() == AnimInstance &&
		Animation == Montage && MontageInstanceId != INDEX_NONE &&
		MontageInstanceId == ActiveMontageInstanceId && MontageInstance &&
		MontageInstance->GetInstanceID() == ActiveMontageInstanceId;
}

bool UAshenOathHealAbility::ApplyConfiguredHealEffect()
{
	const UAshenOathHealActionData* ActionData = ActiveHealData.Get();
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (!ActionData || !ActorInfo)
	{
		return false;
	}

	const FGameplayEffectSpecHandle HealSpec = MakeOutgoingGameplayEffectSpec(
		GetCurrentAbilitySpecHandle(),
		ActorInfo,
		GetCurrentActivationInfo(),
		ActionData->HealEffect,
		GetAbilityLevel()
	);

	return HealSpec.IsValid() && ApplyGameplayEffectSpecToOwner(
		GetCurrentAbilitySpecHandle(),
		ActorInfo,
		GetCurrentActivationInfo(),
		HealSpec
	).WasSuccessfullyApplied();
}

void UAshenOathHealAbility::HandleMontageCompleted()
{
	FinishAbility(false);
}

void UAshenOathHealAbility::HandleMontageInterrupted()
{
	FinishAbility(true);
}

void UAshenOathHealAbility::FinishAbility(const bool bWasCancelled)
{
	if (IsActive() && GetCurrentActorInfo())
	{
		EndAbility(
			GetCurrentAbilitySpecHandle(),
			GetCurrentActorInfo(),
			GetCurrentActivationInfo(),
			true,
			bWasCancelled
		);
	}
}
