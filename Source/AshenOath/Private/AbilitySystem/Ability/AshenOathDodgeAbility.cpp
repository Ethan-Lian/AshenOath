#include "AbilitySystem/Ability/AshenOathDodgeAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystem/Tasks/AshenOathAbilityTask_RecoverFacing.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Data/AshenOathDodgeActionData.h"
#include "Animation/AnimMontage.h"
#include "Defense/CombatDefenseComponent.h"
#include "GameplayTags/AshenOathGameplayTags.h"
#include "Tasks/AbilityTask_ApplyCombatMovement.h"
#include "Targeting/CombatTargetingComponent.h"

UAshenOathDodgeAbility::UAshenOathDodgeAbility()
{
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(AshenOathGameplayTags::Ability_Action);
	AssetTags.AddTag(AshenOathGameplayTags::Ability_Action_Dodge);
	SetAssetTags(AssetTags);

	ActivationOwnedTags.AddTag(AshenOathGameplayTags::State_MovementLocked);
}

bool UAshenOathDodgeAbility::TryActivateWithMovementDirection(
	const FVector& WorldDirection,
	const EAshenOathDodgeFacingMode FacingMode)
{
	PendingMovementDirection = WorldDirection.GetSafeNormal2D();
	PendingFacingMode = FacingMode;

	UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo();
	const FGameplayAbilitySpecHandle SpecHandle = GetCurrentAbilitySpecHandle();
	const bool bActivationAccepted = AbilitySystemComponent &&
		SpecHandle.IsValid() &&
		AbilitySystemComponent->TryActivateAbility(SpecHandle);

	PendingMovementDirection = FVector::ZeroVector;
	PendingFacingMode = EAshenOathDodgeFacingMode::PreserveCurrentFacing;
	return bActivationAccepted;
}

#if WITH_DEV_AUTOMATION_TESTS
void UAshenOathDodgeAbility::TickTasksForTesting(const float DeltaTime)
{
	if (MovementTask)
	{
		MovementTask->TickTask(DeltaTime);
	}

	if (FacingRecoveryTask)
	{
		FacingRecoveryTask->TickTask(DeltaTime);
	}
}
#endif

bool UAshenOathDodgeAbility::CanActivateAbility(
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
		Cast<UAshenOathDodgeActionData>(ResolveActionData(Handle, ActorInfo)),
		ActorInfo,
		PendingMovementDirection
	);
}

void UAshenOathDodgeAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	const UAshenOathDodgeActionData* ActionData =
		Cast<UAshenOathDodgeActionData>(ResolveActionData(Handle, ActorInfo));
	ActiveMovementDirection = PendingMovementDirection;
	ActiveFacingMode = PendingFacingMode;
	PendingMovementDirection = FVector::ZeroVector;
	PendingFacingMode = EAshenOathDodgeFacingMode::PreserveCurrentFacing;

	if (!IsActionDataReady(ActionData, ActorInfo, ActiveMovementDirection))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!StartDodgeMontage(ActorInfo, ActionData))
	{
		FinishAbility(EFinishReason::Cancelled);
		return;
	}

	if (!CommitConfiguredCost(Handle, ActorInfo, ActivationInfo))
	{
		FinishAbility(EFinishReason::Cancelled);
		return;
	}

	double ExecutionStartWorldTime = 0.0;
	if (!StartDefenseWindow(ActorInfo, ActionData, ExecutionStartWorldTime))
	{
		FinishAbility(EFinishReason::Cancelled);
		return;
	}

	ApplyActiveFacing(ActorInfo);
	if (!IsActive())
	{
		return;
	}

	if (!StartFacingRecoveryIfNeeded(ActorInfo, ActionData, ExecutionStartWorldTime))
	{
		FinishAbility(EFinishReason::Cancelled);
		return;
	}

	if (!StartMovementTask(ActionData, ExecutionStartWorldTime))
	{
		FinishAbility(EFinishReason::Cancelled);
	}
}

bool UAshenOathDodgeAbility::StartDodgeMontage(
	const FGameplayAbilityActorInfo* ActorInfo,
	const UAshenOathDodgeActionData* ActionData)
{
	UAbilityTask_PlayMontageAndWait* NewMontageTask =
		UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this,
			TEXT("DodgeMontage"),
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
		return false;
	}

	MontageTask = NewMontageTask;
	NewMontageTask->OnCompleted.AddDynamic(
		this,
		&UAshenOathDodgeAbility::HandleMontageCompleted
	);
	NewMontageTask->OnInterrupted.AddDynamic(
		this,
		&UAshenOathDodgeAbility::HandleMontageInterrupted
	);
	NewMontageTask->OnCancelled.AddDynamic(
		this,
		&UAshenOathDodgeAbility::HandleMontageInterrupted
	);
	NewMontageTask->ReadyForActivation();

	return IsActive() && IsDodgeMontageOwned(ActorInfo, ActionData->Montage);
}

bool UAshenOathDodgeAbility::IsDodgeMontageOwned(
	const FGameplayAbilityActorInfo* ActorInfo,
	const UAnimMontage* Montage)
{
	const UAbilitySystemComponent* AbilitySystemComponent =
		ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	return AbilitySystemComponent &&
		AbilitySystemComponent->IsAnimatingAbility(this) &&
		AbilitySystemComponent->GetCurrentMontage() == Montage;
}

bool UAshenOathDodgeAbility::CommitConfiguredCost(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	ResetCostApplicationResult();
	const bool bCommitAccepted = CommitAbility(Handle, ActorInfo, ActivationInfo);

	// Cost callbacks may synchronously cancel this execution.
	return IsActive() && bCommitAccepted && DidCostApplicationSucceed();
}

bool UAshenOathDodgeAbility::StartDefenseWindow(
	const FGameplayAbilityActorInfo* ActorInfo,
	const UAshenOathDodgeActionData* ActionData,
	double& OutExecutionStartWorldTime)
{
	UCombatDefenseComponent* Defense = ResolveDefenseComponent(ActorInfo);
	UWorld* World = GetWorld();
	if (!Defense || !World)
	{
		return false;
	}

	OutExecutionStartWorldTime = World->GetTimeSeconds();
	ActiveDefenseComponent = Defense;
	ActiveDefenseWindow = Defense->BeginDodgeWindow(
		this,
		ActionData->WindowTags,
		ActionData->WindowStartTime,
		ActionData->WindowDuration,
		OutExecutionStartWorldTime,
		ActionData->PerfectDodgeWindowStartTime,
		ActionData->PerfectDodgeWindowDuration
	);

	const FCombatDefenseWindowHandle Window = ActiveDefenseWindow;
	if (!IsActive() || !Window.IsValid())
	{
		return false;
	}

	// The two-phase start lets EndAbility see the handle before adding a loose
	// tag can synchronously invoke external cancellation logic.
	const bool bActivated = Defense->ActivateDodgeWindow(Window);
	return bActivated && IsActive() && Defense->OwnsWindow(Window);
}

bool UAshenOathDodgeAbility::StartFacingRecoveryIfNeeded(
	const FGameplayAbilityActorInfo* ActorInfo,
	const UAshenOathDodgeActionData* ActionData,
	const double ExecutionStartWorldTime)
{
	AActor* FacingTarget = ActiveFacingMode ==
		EAshenOathDodgeFacingMode::FaceMovementDirection
		? ResolveFacingTarget(ActorInfo)
		: nullptr;
	if (!FacingTarget)
	{
		return true;
	}

	const float RecoveryStartOffset = ActionData->MovementStartTime +
		ActionData->MovementDuration;
	UAshenOathAbilityTask_RecoverFacing* NewFacingRecoveryTask =
		UAshenOathAbilityTask_RecoverFacing::RecoverFacing(
			this,
			TEXT("DodgeFacingRecovery"),
			FacingTarget,
			RecoveryStartOffset,
			FacingRecoveryDuration,
			ExecutionStartWorldTime
		);

	if (!NewFacingRecoveryTask)
	{
		return false;
	}

	FacingRecoveryTask = NewFacingRecoveryTask;
	NewFacingRecoveryTask->ReadyForActivation();
	return IsActive();
}

bool UAshenOathDodgeAbility::StartMovementTask(
	const UAshenOathDodgeActionData* ActionData,
	const double ExecutionStartWorldTime)
{
	UAbilityTask_ApplyCombatMovement* NewMovementTask =
		UAbilityTask_ApplyCombatMovement::ApplyCombatMovement(
			this,
			TEXT("DodgeMovement"),
			ActiveMovementDirection,
			ActionData->MovementDistance,
			ActionData->MovementStartTime,
			ActionData->MovementDuration,
			ExecutionStartWorldTime
		);

	if (!NewMovementTask)
	{
		return false;
	}

	MovementTask = NewMovementTask;
	NewMovementTask->OnFailed.AddDynamic(
		this,
		&UAshenOathDodgeAbility::HandleMovementFailed
	);
	NewMovementTask->ReadyForActivation();
	return IsActive();
}

void UAshenOathDodgeAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	UCombatDefenseComponent* Defense = ActiveDefenseComponent.Get();
	const FCombatDefenseWindowHandle WindowToEnd = ActiveDefenseWindow;

	// Revoke the reusable ability instance's ownership before touching external
	// state. Removing tags and stopping tasks may synchronously re-enter here.
	ActiveDefenseWindow.Reset();
	ActiveDefenseComponent.Reset();
	MontageTask = nullptr;
	MovementTask = nullptr;
	FacingRecoveryTask = nullptr;
	ActiveMovementDirection = FVector::ZeroVector;
	PendingMovementDirection = FVector::ZeroVector;
	ActiveFacingMode = EAshenOathDodgeFacingMode::PreserveCurrentFacing;
	PendingFacingMode = EAshenOathDodgeFacingMode::PreserveCurrentFacing;
	ResetCostApplicationResult();

	if (Defense)
	{
		Defense->EndDodgeWindow(WindowToEnd);
	}

	Super::EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		bReplicateEndAbility,
		bWasCancelled
	);
}

bool UAshenOathDodgeAbility::IsActionDataReady(
	const UAshenOathDodgeActionData* ActionData,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FVector& MovementDirection) const
{
	if (!ActionData || !ActorInfo)
	{
		return false;
	}

	const bool bMontageReady = ActionData->Montage &&
		ActionData->PlayRate > KINDA_SMALL_NUMBER;
	const bool bMovementReady =
		ActionData->MovementDistance > KINDA_SMALL_NUMBER &&
		ActionData->MovementStartTime >= 0.0f &&
		ActionData->MovementDuration > KINDA_SMALL_NUMBER &&
		FMath::IsFinite(FacingRecoveryDuration) &&
		FacingRecoveryDuration > KINDA_SMALL_NUMBER &&
		MovementDirection.SizeSquared2D() > SMALL_NUMBER;
	const bool bWindowReady =
		ActionData->WindowStartTime >= 0.0f &&
		ActionData->WindowDuration > KINDA_SMALL_NUMBER &&
		ActionData->PerfectDodgeWindowStartTime >= 0.0f &&
		ActionData->PerfectDodgeWindowDuration >= 0.0f &&
		ActionData->WindowTags.HasTagExact(
			AshenOathGameplayTags::State_Invulnerable);
	const bool bAnimationSourceReady = ActorInfo->GetAnimInstance() &&
		ActorInfo->SkeletalMeshComponent.IsValid();

	if (!bMontageReady || !bMovementReady || !bWindowReady ||
		!bAnimationSourceReady)
	{
		return false;
	}

	if (!ActionData->StartSection.IsNone() &&
		!ActionData->Montage->IsValidSectionName(ActionData->StartSection))
	{
		return false;
	}

	const UCombatDefenseComponent* Defense = ResolveDefenseComponent(ActorInfo);
	return Defense && Defense->CanBeginDodgeWindow(
		ActionData->WindowTags,
		ActionData->WindowStartTime,
		ActionData->WindowDuration,
		ActionData->PerfectDodgeWindowStartTime,
		ActionData->PerfectDodgeWindowDuration
	);
}

UCombatDefenseComponent* UAshenOathDodgeAbility::ResolveDefenseComponent(
	const FGameplayAbilityActorInfo* ActorInfo) const
{
	AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	return IsValid(AvatarActor)
		? AvatarActor->FindComponentByClass<UCombatDefenseComponent>()
		: nullptr;
}

AActor* UAshenOathDodgeAbility::ResolveFacingTarget(
	const FGameplayAbilityActorInfo* ActorInfo) const
{
	AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	const UCombatTargetingComponent* Targeting = IsValid(AvatarActor)
		? AvatarActor->FindComponentByClass<UCombatTargetingComponent>()
		: nullptr;
	return Targeting ? Targeting->GetTarget() : nullptr;
}

void UAshenOathDodgeAbility::ApplyActiveFacing(
	const FGameplayAbilityActorInfo* ActorInfo) const
{
	if (ActiveFacingMode != EAshenOathDodgeFacingMode::FaceMovementDirection)
	{
		return;
	}

	AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	if (!IsValid(AvatarActor) || AvatarActor->IsActorBeingDestroyed())
	{
		return;
	}

	const float DesiredYaw = ActiveMovementDirection.Rotation().Yaw;
	AvatarActor->SetActorRotation(FRotator(0.0f, DesiredYaw, 0.0f));
}

void UAshenOathDodgeAbility::HandleMontageCompleted()
{
	FinishAbility(EFinishReason::Completed);
}

void UAshenOathDodgeAbility::HandleMontageInterrupted()
{
	FinishAbility(EFinishReason::Cancelled);
}

void UAshenOathDodgeAbility::HandleMovementFailed()
{
	FinishAbility(EFinishReason::Cancelled);
}

void UAshenOathDodgeAbility::FinishAbility(const EFinishReason Reason)
{
	if (!IsActive())
	{
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (!ActorInfo)
	{
		return;
	}

	EndAbility(
		GetCurrentAbilitySpecHandle(),
		ActorInfo,
		GetCurrentActivationInfo(),
		true,
		Reason == EFinishReason::Cancelled
	);
}
