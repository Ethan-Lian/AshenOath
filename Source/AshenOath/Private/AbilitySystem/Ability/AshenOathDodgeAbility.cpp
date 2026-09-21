#include "AbilitySystem/Ability/AshenOathDodgeAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystemComponent.h"
#include "Actions/CombatActionData.h"
#include "Animation/AnimMontage.h"
#include "Defense/CombatDefenseComponent.h"
#include "GameplayTags/AshenOathGameplayTags.h"
#include "Tasks/AbilityTask_ApplyCombatMovement.h"

UAshenOathDodgeAbility::UAshenOathDodgeAbility()
{
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(AshenOathGameplayTags::Ability_Action);
	AssetTags.AddTag(AshenOathGameplayTags::Ability_Action_Dodge);
	SetAssetTags(AssetTags);
}

bool UAshenOathDodgeAbility::TryActivateWithMovementDirection(
	const FVector& WorldDirection)
{
	PendingMovementDirection = WorldDirection.GetSafeNormal2D();

	UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo();
	const FGameplayAbilitySpecHandle SpecHandle = GetCurrentAbilitySpecHandle();
	const bool bActivationAccepted = AbilitySystemComponent &&
		SpecHandle.IsValid() &&
		AbilitySystemComponent->TryActivateAbility(SpecHandle);

	PendingMovementDirection = FVector::ZeroVector;
	return bActivationAccepted;
}

#if WITH_DEV_AUTOMATION_TESTS
void UAshenOathDodgeAbility::TickMovementTaskForTesting(const float DeltaTime)
{
	if (MovementTask)
	{
		MovementTask->TickTask(DeltaTime);
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
		ResolveActionData(Handle, ActorInfo),
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
	const UCombatActionData* ActionData = ResolveActionData(Handle, ActorInfo);
	ActiveMovementDirection = PendingMovementDirection;
	PendingMovementDirection = FVector::ZeroVector;

	if (!IsActionDataReady(ActionData, ActorInfo, ActiveMovementDirection))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

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
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
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

	if (!IsActive())
	{
		return;
	}

	UAbilitySystemComponent* AbilitySystemComponent =
		ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;

	if (!AbilitySystemComponent ||
		!AbilitySystemComponent->IsAnimatingAbility(this) ||
		AbilitySystemComponent->GetCurrentMontage() != ActionData->Montage)
	{
		FinishAbility(true);
		return;
	}

	ResetCostApplicationResult();
	const bool bCommitAccepted = CommitAbility(Handle, ActorInfo, ActivationInfo);

	// Cost callbacks may synchronously cancel this execution.
	if (!IsActive())
	{
		return;
	}

	if (!bCommitAccepted || !DidCostApplicationSucceed())
	{
		FinishAbility(true);
		return;
	}

	UCombatDefenseComponent* Defense = ResolveDefenseComponent(ActorInfo);
	UWorld* World = GetWorld();
	if (!Defense || !World)
	{
		FinishAbility(true);
		return;
	}

	const double ExecutionStartWorldTime = World->GetTimeSeconds();
	ActiveDefenseComponent = Defense;
	ActiveDefenseWindow = Defense->BeginDodgeWindow(
		this,
		ActionData->WindowTags,
		ActionData->WindowStartTime,
		ActionData->WindowDuration,
		ExecutionStartWorldTime,
		ActionData->PerfectDodgeWindowStartTime,
		ActionData->PerfectDodgeWindowDuration
	);

	if (!ActiveDefenseWindow.IsValid())
	{
		FinishAbility(true);
		return;
	}

	// The two-phase start lets EndAbility see the handle before adding a loose
	// tag can synchronously invoke external cancellation logic.
	if (!Defense->ActivateDodgeWindow(ActiveDefenseWindow) ||
		!IsActive() ||
		!Defense->OwnsWindow(ActiveDefenseWindow))
	{
		if (IsActive())
		{
			FinishAbility(true);
		}
		return;
	}
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
		FinishAbility(true);
		return;
	}

	MovementTask = NewMovementTask;
	NewMovementTask->OnFailed.AddDynamic(
		this,
		&UAshenOathDodgeAbility::HandleMovementFailed
	);
	NewMovementTask->ReadyForActivation();
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
	ActiveMovementDirection = FVector::ZeroVector;
	PendingMovementDirection = FVector::ZeroVector;
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
	const UCombatActionData* ActionData,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FVector& MovementDirection) const
{
	if (!ActionData ||
		!ActionData->Montage ||
		ActionData->PlayRate <= KINDA_SMALL_NUMBER ||
		ActionData->MovementDistance <= KINDA_SMALL_NUMBER ||
		ActionData->MovementStartTime < 0.0f ||
		ActionData->MovementDuration <= KINDA_SMALL_NUMBER ||
		MovementDirection.SizeSquared2D() <= SMALL_NUMBER ||
		ActionData->WindowStartTime < 0.0f ||
		ActionData->WindowDuration <= KINDA_SMALL_NUMBER ||
		ActionData->PerfectDodgeWindowStartTime < 0.0f ||
		ActionData->PerfectDodgeWindowDuration < 0.0f ||
		!ActionData->WindowTags.HasTagExact(
			AshenOathGameplayTags::State_Invulnerable) ||
		!ActorInfo ||
		!ActorInfo->GetAnimInstance() ||
		!ActorInfo->SkeletalMeshComponent.IsValid())
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

void UAshenOathDodgeAbility::HandleMontageCompleted()
{
	FinishAbility(false);
}

void UAshenOathDodgeAbility::HandleMontageInterrupted()
{
	FinishAbility(true);
}

void UAshenOathDodgeAbility::HandleMovementFailed()
{
	FinishAbility(true);
}

void UAshenOathDodgeAbility::FinishAbility(const bool bWasCancelled)
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
		bWasCancelled
	);
}
