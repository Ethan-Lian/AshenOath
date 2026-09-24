#include "AbilitySystem/Ability/AshenOathBossDashSwingAbility.h"

#include "AbilitySystem/Data/AshenOathBossDashSwingActionData.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Kismet/GameplayStatics.h"
#include "GameplayTags/AshenOathGameplayTags.h"
#include "Tasks/AbilityTask_ApplyCombatMovement.h"

UAshenOathBossDashSwingAbility::UAshenOathBossDashSwingAbility()
{
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(AshenOathGameplayTags::Ability_Action);
	AssetTags.AddTag(AshenOathGameplayTags::Ability_Action_BossDashSwing);
	SetAssetTags(AssetTags);
}

bool UAshenOathBossDashSwingAbility::CanActivateAbility(
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

	const UAshenOathBossDashSwingActionData* ActionData =
		Cast<UAshenOathBossDashSwingActionData>(ResolveActionData(Handle, ActorInfo));
	const AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	const APawn* Target = Avatar ? UGameplayStatics::GetPlayerPawn(Avatar, 0) : nullptr;
	return IsDashSwingDataReady(ActionData) && IsValid(Avatar) && IsValid(Target) &&
		FVector::Dist2D(Avatar->GetActorLocation(), Target->GetActorLocation()) >
			ActionData->StopDistance + KINDA_SMALL_NUMBER;
}

void UAshenOathBossDashSwingAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	const UAshenOathBossDashSwingActionData* ActionData =
		Cast<UAshenOathBossDashSwingActionData>(ResolveActionData(Handle, ActorInfo));
	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	APawn* Target = Avatar ? UGameplayStatics::GetPlayerPawn(Avatar, 0) : nullptr;
	UWorld* World = GetWorld();
	if (!IsDashSwingDataReady(ActionData) || !IsValid(Avatar) ||
		!IsValid(Target) || !World)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FVector ToTarget = Target->GetActorLocation() - Avatar->GetActorLocation();
	const float Distance = FMath::Min(
		FMath::Max(ToTarget.Size2D() - ActionData->StopDistance, 0.0f),
		ActionData->MaxDashDistance);
	if (Distance <= KINDA_SMALL_NUMBER)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ActiveActionData = ActionData;
	if (!StartAttackMontage(ActorInfo, ActionData, ActionData->DashSection))
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
		ActionData->DashSection, ActionData->DashSection, Montage);

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

	UAbilityTask_ApplyCombatMovement* NewMovementTask =
		UAbilityTask_ApplyCombatMovement::ApplyCombatMovement(
			this, TEXT("BossDashMovement"), ToTarget.GetSafeNormal2D(),
			Distance, 0.0f, ActionData->DashDurationSeconds,
			World->GetTimeSeconds());
	if (!NewMovementTask)
	{
		FinishAbility(true);
		return;
	}

	MovementTask = NewMovementTask;
	NewMovementTask->OnCompleted.AddDynamic(
		this, &UAshenOathBossDashSwingAbility::HandleDashCompleted);
	NewMovementTask->OnFailed.AddDynamic(
		this, &UAshenOathBossDashSwingAbility::HandleDashFailed);
	NewMovementTask->ReadyForActivation();
}

void UAshenOathBossDashSwingAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	MovementTask = nullptr;
	ActiveActionData.Reset();
	Super::EndAbility(
		Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

bool UAshenOathBossDashSwingAbility::IsDashSwingDataReady(
	const UAshenOathBossDashSwingActionData* ActionData) const
{
	return ActionData && ActionData->Montage &&
		!ActionData->DashSection.IsNone() &&
		!ActionData->SwingSection.IsNone() &&
		ActionData->DashSection != ActionData->SwingSection &&
		ActionData->Montage->IsValidSectionName(ActionData->DashSection) &&
		ActionData->Montage->IsValidSectionName(ActionData->SwingSection) &&
		ActionData->MaxDashDistance > KINDA_SMALL_NUMBER &&
		ActionData->StopDistance >= 0.0f &&
		ActionData->DashDurationSeconds > KINDA_SMALL_NUMBER &&
		(ActionData->StartSection.IsNone() ||
			ActionData->StartSection == ActionData->DashSection);
}

void UAshenOathBossDashSwingAbility::HandleDashCompleted()
{
	if (!IsActive())
	{
		return;
	}

	const UAshenOathBossDashSwingActionData* ActionData = ActiveActionData.Get();
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

void UAshenOathBossDashSwingAbility::HandleDashFailed()
{
	FinishAbility(true);
}
