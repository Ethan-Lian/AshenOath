#include "AbilitySystem/Ability/AshenOathBossDashSwingAbility.h"

#include "AbilitySystem/Data/AshenOathBossDashSwingActionData.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "AbilitySystem/Tasks/AbilityTask_BossDashChase.h"
#include "Kismet/GameplayStatics.h"
#include "GameplayTags/AshenOathGameplayTags.h"

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
	if (!IsDashSwingDataReady(ActionData) || !IsValid(Avatar) || !IsValid(Target))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (FVector::Dist2D(Avatar->GetActorLocation(), Target->GetActorLocation()) <=
		ActionData->StopDistance + KINDA_SMALL_NUMBER)
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

	UAbilityTask_BossDashChase* NewMovementTask =
		UAbilityTask_BossDashChase::ChaseTarget(
			this, TEXT("BossDashChase"), Target,
			ActionData->StopDistance, ActionData->DashSpeedMultiplier);
	if (!NewMovementTask)
	{
		FinishAbility(true);
		return;
	}

	MovementTask = NewMovementTask;
	NewMovementTask->OnReached.AddDynamic(
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
		ActionData->StopDistance > KINDA_SMALL_NUMBER &&
		FMath::IsFinite(ActionData->DashSpeedMultiplier) &&
		ActionData->DashSpeedMultiplier > 1.0f &&
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

	AActor* Avatar = ActorInfo->AvatarActor.Get();
	const APawn* Target = Avatar ? UGameplayStatics::GetPlayerPawn(Avatar, 0) : nullptr;
	if (!IsValid(Avatar) || !IsValid(Target) ||
		FVector::Dist2D(Avatar->GetActorLocation(), Target->GetActorLocation()) >
			ActionData->StopDistance + KINDA_SMALL_NUMBER)
	{
		FinishAbility(true);
		return;
	}

	const FVector ToTarget = Target->GetActorLocation() - Avatar->GetActorLocation();
	if (!ToTarget.IsNearlyZero())
	{
		Avatar->SetActorRotation(FRotator(0.0f, ToTarget.Rotation().Yaw, 0.0f));
	}

	if (!BeginMeleeExecution(
		GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(),
		ActionData, EMeleeCostCommit::SkipConfigured))
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
