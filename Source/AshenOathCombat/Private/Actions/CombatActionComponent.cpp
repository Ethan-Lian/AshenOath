#include "Actions/CombatActionComponent.h"

#include "Actions/CombatActionData.h"
#include "Actions/CombatMeleeComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"

UCombatActionComponent::UCombatActionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCombatActionComponent::BeginPlay()
{
	Super::BeginPlay();

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	CachedCharacter = Character;

	if (IsValid(Character))
	{
		MeleeComponent = Character->FindComponentByClass<UCombatMeleeComponent>();
	}
}

void UCombatActionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelCurrentAction(0.0f);
	MeleeComponent.Reset();
	CachedCharacter.Reset();

	Super::EndPlay(EndPlayReason);
}

ECombatActionStartResult UCombatActionComponent::TryStartAction(const UCombatActionData* ActionData,
	                                                            FCombatActionHandle& OutHandle)
{
	// OutHandle belongs to the caller and may contain a handle from an earlier request.
	// Reset it first so a rejected request always returns an invalid handle.
	// A successful request will replace it with the new action handle.
	OutHandle.Reset();

	if (CurrentAction.IsValid())
	{
		return ECombatActionStartResult::RejectedAlreadyActive;
	}

	ACharacter* Character = CachedCharacter.Get();

	if (!Character || Character->IsActorBeingDestroyed())
	{
		return ECombatActionStartResult::RejectedInvalidOwner;
	}

	if (!ActionData || !ActionData->Montage || ActionData->PlayRate <= 0.0f)
	{
		return ECombatActionStartResult::RejectedInvalidData;
	}

	if (!ActionData->StartSection.IsNone() &&
	    ActionData->Montage->GetSectionIndex(ActionData->StartSection) == INDEX_NONE)
	{
		return ECombatActionStartResult::RejectedInvalidData;
	}

	USkeletalMeshComponent* MeshComponent = Character->GetMesh();
	UAnimInstance* AnimInstance = MeshComponent ? MeshComponent->GetAnimInstance() : nullptr;

	if (!AnimInstance)
	{
		return ECombatActionStartResult::RejectedInvalidAnimation;
	}

	const float PlayedDuration = AnimInstance->Montage_Play(ActionData->Montage, ActionData->PlayRate);

	if (PlayedDuration <= 0.0f)
	{
		return ECombatActionStartResult::RejectedMontageFailed;
	}

	CurrentAction.Value = AllocateActionInstanceId();
	ActiveAnimInstance = AnimInstance;
	ActiveMontage = ActionData->Montage;

	// The action owns the execution handle. Melee receives a value snapshot so
	// hit detection never depends on mutable shared Data Asset state mid-action.
	if (UCombatMeleeComponent* Melee = MeleeComponent.Get())
	{
		Melee->BeginAction(CurrentAction, ActionData->DamageEffect, ActionData->MeleeTraceRadius,
		                   ActionData->MeleeTraceBones, ActionData->bCanTriggerPerfectDodge);
	}

	if (!ActionData->StartSection.IsNone())
	{
		AnimInstance->Montage_JumpToSection(ActionData->StartSection, ActionData->Montage);
	}

	FOnMontageEnded EndDelegate;

	// FOnMontageEnded supplies Montage and bInterrupted. BindUObject stores
	// this execution ID as an additional payload for stale-callback checks.
	EndDelegate.BindUObject(this, &UCombatActionComponent::HandleMontageEnded, CurrentAction.Value);

	AnimInstance->Montage_SetEndDelegate(EndDelegate, ActionData->Montage);

	OutHandle = CurrentAction;

	return ECombatActionStartResult::Started;
}

void UCombatActionComponent::CancelCurrentAction(float BlendOutTime)
{
	if (!CurrentAction.IsValid())
	{
		return;
	}

	FinishAction(CurrentAction.Value, true, BlendOutTime);
}

bool UCombatActionComponent::IsActionActive() const
{
	return CurrentAction.IsValid();
}

int32 UCombatActionComponent::AllocateActionInstanceId()
{
	const int32 AllocatedId = NextActionInstanceId;

	NextActionInstanceId = NextActionInstanceId == MAX_int32 ? 1 : NextActionInstanceId + 1;

	return AllocatedId;
}

void UCombatActionComponent::HandleMontageEnded(UAnimMontage*, bool, int32 ExpectedActionId)
{
	FinishAction(ExpectedActionId, false, 0.0f);
}

void UCombatActionComponent::FinishAction(int32 ExpectedActionId, bool bStopMontage, float BlendOutTime)
{
	// Only the current action may clear its state.
	// Ignore callbacks whose ID does not match the active action.
	if (!CurrentAction.IsValid() || CurrentAction.Value != ExpectedActionId)
	{
		return;
	}

	UAnimInstance* AnimInstance = ActiveAnimInstance.Get();
	UAnimMontage* AnimMontage = ActiveMontage.Get();

	// End dependent action-scoped state while the handle is still valid. The
	// melee component rejects stale cleanup requests using the same handle.
	if (UCombatMeleeComponent* Melee = MeleeComponent.Get())
	{
		Melee->EndAction(CurrentAction);
	}

	// Clear the component's action state before calling animation functions.
	// Stopping a Montage may trigger callbacks that re-enter this component.
	ActiveMontage = nullptr;
	ActiveAnimInstance.Reset();
	CurrentAction.Reset();

	if (!AnimInstance || !AnimMontage)
	{
		return;
	}

	// Explicit cancellation calls Montage_Stop, which may later invoke the end callback.
	// The action state has already been cleared, so remove the callback before stopping the Montage.
	FOnMontageEnded EmptyDelegate;

	AnimInstance->Montage_SetEndDelegate(EmptyDelegate, AnimMontage);

	if (bStopMontage && AnimInstance->Montage_IsActive(AnimMontage))
	{
		AnimInstance->Montage_Stop(FMath::Max(BlendOutTime, 0.0f), AnimMontage);
	}
}

void UCombatActionComponent::BeginMeleeHitWindow(int32 NotifyInstanceId, int32 DamageSegmentId)
{
	if (UCombatMeleeComponent* Melee = MeleeComponent.Get())
	{
		Melee->BeginHitWindow(CurrentAction, NotifyInstanceId, DamageSegmentId);
	}
}

void UCombatActionComponent::EndMeleeHitWindow(int32 NotifyInstanceId)
{
	if (UCombatMeleeComponent* Melee = MeleeComponent.Get())
	{
		Melee->EndHitWindow(CurrentAction, NotifyInstanceId);
	}
}

void UCombatActionComponent::TickMeleeHitWindow(const int32 NotifyInstanceId)
{
	if (UCombatMeleeComponent* Melee = MeleeComponent.Get())
	{
		Melee->TickHitWindow(CurrentAction, NotifyInstanceId);
	}
}

bool UCombatActionComponent::IsMeleeHitWindowActive() const
{
	const UCombatMeleeComponent* Melee = MeleeComponent.Get();

	return Melee && Melee->IsHitWindowActive(CurrentAction);
}
