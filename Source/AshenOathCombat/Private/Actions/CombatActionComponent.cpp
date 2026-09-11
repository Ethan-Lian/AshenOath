#include "Actions/CombatActionComponent.h"

#include "Actions/CombatActionData.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "CollisionQueryParams.h"
#include "Components/SkeletalMeshComponent.h"
#include "Damage/CombatDamageComponent.h"
#include "Damage/CombatDamageTypes.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameplayEffect.h"

UCombatActionComponent::UCombatActionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCombatActionComponent::BeginPlay()
{
	Super::BeginPlay();

	CachedCharacter = Cast<ACharacter>(GetOwner());
}

void UCombatActionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelCurrentAction(0.0f);
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
	ActiveDamageEffect = ActionData->DamageEffect;
	ActiveMeleeTraceRadius = FMath::Max(ActionData->MeleeTraceRadius, 0.0f);
	ActiveMeleeTraceBones = ActionData->MeleeTraceBones;
	bActiveDamageCanTriggerPerfectDodge = ActionData->bCanTriggerPerfectDodge;

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

	ResetMeleeHitWindow();
	ResetActiveActionDamageData();

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

void UCombatActionComponent::ResetActiveActionDamageData()
{
	ActiveDamageEffect = nullptr;
	ActiveMeleeTraceRadius = 0.0f;
	ActiveMeleeTraceBones.Reset();
	bActiveDamageCanTriggerPerfectDodge = false;
}

void UCombatActionComponent::BeginMeleeHitWindow(int32 NotifyInstanceId, int32 DamageSegmentId)
{
	if (!CurrentAction.IsValid() || DamageSegmentId <= 0)
	{
		return;
	}

	if (IsMeleeHitWindowActive())
	{
		return;
	}

	ACharacter* Character = CachedCharacter.Get();
	USkeletalMeshComponent* MeshComponent = Character ? Character->GetMesh() : nullptr;

	if (!IsValid(MeshComponent) || !ActiveDamageEffect || ActiveMeleeTraceRadius <= 0.0f ||
	    ActiveMeleeTraceBones.Num() < 2)
	{
		return;
	}

	PreviousMeleeTraceLocations.SetNumUninitialized(ActiveMeleeTraceBones.Num());

	for (int32 Index = 0; Index < ActiveMeleeTraceBones.Num(); ++Index)
	{
		const FName TracePointName = ActiveMeleeTraceBones[Index];

		if (!MeshComponent->DoesSocketExist(TracePointName))
		{
			ResetMeleeHitWindow();
			return;
		}

		PreviousMeleeTraceLocations[Index] = MeshComponent->GetSocketLocation(TracePointName);
	}

	ActiveHitWindowActionInstanceId = CurrentAction.Value;
	ActiveHitWindowNotifyInstanceId = NotifyInstanceId;
	ActiveDamageSegmentId = DamageSegmentId;
	HitActorsInCurrentWindow.Reset();
}

void UCombatActionComponent::EndMeleeHitWindow(int32 NotifyInstanceId)
{
	if (!CurrentAction.IsValid() || ActiveHitWindowActionInstanceId != CurrentAction.Value ||
	    ActiveHitWindowNotifyInstanceId != NotifyInstanceId)
	{
		return;
	}

	ResetMeleeHitWindow();
}

void UCombatActionComponent::TickMeleeHitWindow()
{
	if (!IsMeleeHitWindowActive())
	{
		return;
	}

	ACharacter* Character = CachedCharacter.Get();
	USkeletalMeshComponent* MeshComponent = Character ? Character->GetMesh() : nullptr;

	if (!IsValid(MeshComponent) || PreviousMeleeTraceLocations.Num() != ActiveMeleeTraceBones.Num())
	{
		ResetMeleeHitWindow();
		return;
	}

	TArray<FVector> CurrentTraceLocations;
	CurrentTraceLocations.SetNumUninitialized(ActiveMeleeTraceBones.Num());

	for (int32 Index = 0; Index < ActiveMeleeTraceBones.Num(); ++Index)
	{
		const FName TracePointName = ActiveMeleeTraceBones[Index];

		if (!MeshComponent->DoesSocketExist(TracePointName))
		{
			ResetMeleeHitWindow();
			return;
		}

		CurrentTraceLocations[Index] = MeshComponent->GetSocketLocation(TracePointName);
	}

	// Sweep every sample point from its previous position to its current one.
	for (int32 Index = 0; Index < CurrentTraceLocations.Num(); ++Index)
	{
		SweepMeleeSegment(PreviousMeleeTraceLocations[Index], CurrentTraceLocations[Index]);

		if (!IsMeleeHitWindowActive())
		{
			return;
		}
	}

	// Sweep between adjacent trace points at the weapon's current pose.
	// This fills the gaps between individual trace points so the weapon body
	// can hit targets, not just the sampled sockets/bones.
	for (int32 Index = 1; Index < CurrentTraceLocations.Num(); ++Index)
	{
		SweepMeleeSegment(CurrentTraceLocations[Index - 1], CurrentTraceLocations[Index]);

		if (!IsMeleeHitWindowActive())
		{
			return;
		}
	}

	PreviousMeleeTraceLocations = MoveTemp(CurrentTraceLocations);
}

bool UCombatActionComponent::IsMeleeHitWindowActive() const
{
	return CurrentAction.IsValid() && ActiveHitWindowActionInstanceId == CurrentAction.Value;
}

void UCombatActionComponent::ResetMeleeHitWindow()
{
	ActiveHitWindowActionInstanceId = 0;
	ActiveDamageSegmentId = 0;
	ActiveHitWindowNotifyInstanceId = INDEX_NONE;

	PreviousMeleeTraceLocations.Reset();
	HitActorsInCurrentWindow.Reset();
}

void UCombatActionComponent::SweepMeleeSegment(const FVector& Start, const FVector& End)
{
	ACharacter* Character = CachedCharacter.Get();
	UWorld* World = GetWorld();

	if (!IsValid(Character) || !World || !IsMeleeHitWindowActive())
	{
		return;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AshenOathMeleeSweep), false, Character);

	const FCollisionObjectQueryParams ObjectQueryParams(ECC_Pawn);

	TArray<FHitResult> HitResults;

	World->SweepMultiByObjectType(HitResults, Start, End, FQuat::Identity, ObjectQueryParams,
	                              FCollisionShape::MakeSphere(ActiveMeleeTraceRadius), QueryParams);

	for (const FHitResult& HitResult : HitResults)
	{
		if (!IsMeleeHitWindowActive())
		{
			return;
		}

		SubmitMeleeHit(HitResult.GetActor());
	}
}

void UCombatActionComponent::SubmitMeleeHit(AActor* HitActor)
{
	ACharacter* Character = CachedCharacter.Get();

	if (!IsMeleeHitWindowActive() || !IsValid(Character) || !IsValid(HitActor) || HitActor == Character)
	{
		return;
	}

	const TWeakObjectPtr<AActor> HitActorKey(HitActor);

	if (HitActorsInCurrentWindow.Contains(HitActorKey))
	{
		return;
	}

	UCombatDamageComponent* DamageComponent = HitActor->FindComponentByClass<UCombatDamageComponent>();

	if (!DamageComponent)
	{
		return;
	}

	// Register before applying the effect because GAS callbacks are synchronous
	// and may re-enter combat code.
	HitActorsInCurrentWindow.Add(HitActorKey);

	FCombatDamageAttempt DamageAttempt;
	DamageAttempt.SourceActor = Character;
	DamageAttempt.DamageEffect = ActiveDamageEffect;
	DamageAttempt.AttackInstanceId = ActiveHitWindowActionInstanceId;
	DamageAttempt.HitId = ActiveDamageSegmentId;
	DamageAttempt.HitTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	DamageAttempt.bCanTriggerPerfectDodge = bActiveDamageCanTriggerPerfectDodge;

	DamageComponent->ApplyDamageAttempt(DamageAttempt);
}
