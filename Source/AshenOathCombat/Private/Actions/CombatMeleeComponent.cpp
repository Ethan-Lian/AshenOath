#include "Actions/CombatMeleeComponent.h"

#include "CollisionQueryParams.h"
#include "Components/SkeletalMeshComponent.h"
#include "Damage/CombatDamageComponent.h"
#include "Damage/CombatDamageTypes.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameplayEffect.h"

UCombatMeleeComponent::UCombatMeleeComponent()
{
	// Animation Notify State callbacks drive sampling, so this component does not
	// need an independent per-frame tick outside an active hit window.
	PrimaryComponentTick.bCanEverTick = false;
}

void UCombatMeleeComponent::BeginPlay()
{
	Super::BeginPlay();

	CachedCharacter = Cast<ACharacter>(GetOwner());
}

void UCombatMeleeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// EndPlay can occur while a Montage or Notify State is still active. Clear all
	// runtime state here so delayed animation callbacks cannot retain a hit window.
	ResetAction();
	CachedCharacter.Reset();

	Super::EndPlay(EndPlayReason);
}

void UCombatMeleeComponent::BeginAction(const FCombatActionHandle& ActionHandle,
	                                     TSubclassOf<UGameplayEffect> DamageEffect,
	                                     const float TraceRadius,
	                                     const TArray<FName>& TraceBones,
	                                     const bool bCanTriggerPerfectDodge)
{
	// The action component permits only one active execution. Reset first so this
	// snapshot and all per-window state always belong to the supplied handle.
	ResetAction();

	if (!ActionHandle.IsValid())
	{
		return;
	}

	ActiveActionInstanceId = ActionHandle.Value;
	ActiveDamageEffect = DamageEffect;
	ActiveMeleeTraceRadius = FMath::Max(TraceRadius, 0.0f);
	ActiveMeleeTraceBones = TraceBones;
	bActiveDamageCanTriggerPerfectDodge = bCanTriggerPerfectDodge;
}

void UCombatMeleeComponent::EndAction(const FCombatActionHandle& ActionHandle)
{
	if (!ActionHandle.IsValid() || ActiveActionInstanceId != ActionHandle.Value)
	{
		return;
	}

	ResetAction();
}

void UCombatMeleeComponent::BeginHitWindow(const FCombatActionHandle& ActionHandle,
	                                        const int32 NotifyInstanceId,
	                                        const int32 DamageSegmentId)
{
	if (!ActionHandle.IsValid() || ActiveActionInstanceId != ActionHandle.Value ||
	    HasActiveHitWindow() || NotifyInstanceId == INDEX_NONE || DamageSegmentId <= 0)
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
			ResetHitWindow();
			return;
		}

		PreviousMeleeTraceLocations[Index] = MeshComponent->GetSocketLocation(TracePointName);
	}

	ActiveHitWindowNotifyInstanceId = NotifyInstanceId;
	ActiveDamageSegmentId = DamageSegmentId;
	HitActorsInCurrentWindow.Reset();
}

void UCombatMeleeComponent::EndHitWindow(const FCombatActionHandle& ActionHandle,const int32 NotifyInstanceId)
{
	if (!IsHitWindowActive(ActionHandle) || ActiveHitWindowNotifyInstanceId != NotifyInstanceId)
	{
		return;
	}

	ResetHitWindow();
}

void UCombatMeleeComponent::TickHitWindow(const FCombatActionHandle& ActionHandle,
	                                      const int32 NotifyInstanceId)
{
	if (!OwnsHitWindow(ActionHandle, NotifyInstanceId))
	{
		return;
	}

	ACharacter* Character = CachedCharacter.Get();
	USkeletalMeshComponent* MeshComponent = Character ? Character->GetMesh() : nullptr;

	if (!IsValid(MeshComponent) || PreviousMeleeTraceLocations.Num() != ActiveMeleeTraceBones.Num())
	{
		ResetHitWindow();
		return;
	}

	TArray<FVector> CurrentTraceLocations;
	CurrentTraceLocations.SetNumUninitialized(ActiveMeleeTraceBones.Num());

	for (int32 Index = 0; Index < ActiveMeleeTraceBones.Num(); ++Index)
	{
		const FName TracePointName = ActiveMeleeTraceBones[Index];

		if (!MeshComponent->DoesSocketExist(TracePointName))
		{
			ResetHitWindow();
			return;
		}

		CurrentTraceLocations[Index] = MeshComponent->GetSocketLocation(TracePointName);
	}

	// Sweep every sample point from its previous position to its current one.
	for (int32 Index = 0; Index < CurrentTraceLocations.Num(); ++Index)
	{
		SweepMeleeSegment(ActionHandle, NotifyInstanceId,
		                  PreviousMeleeTraceLocations[Index], CurrentTraceLocations[Index]);

		// Applying damage may synchronously re-enter combat code and end the action.
		if (!OwnsHitWindow(ActionHandle, NotifyInstanceId))
		{
			return;
		}
	}

	// Sweep between adjacent trace points at the weapon's current pose.
	// This fills the gaps between individual trace points so the weapon body
	// can hit targets, not just the sampled sockets/bones.
	for (int32 Index = 1; Index < CurrentTraceLocations.Num(); ++Index)
	{
		SweepMeleeSegment(ActionHandle, NotifyInstanceId,
		                  CurrentTraceLocations[Index - 1], CurrentTraceLocations[Index]);

		if (!OwnsHitWindow(ActionHandle, NotifyInstanceId))
		{
			return;
		}
	}

	PreviousMeleeTraceLocations = MoveTemp(CurrentTraceLocations);
}

bool UCombatMeleeComponent::IsHitWindowActive(const FCombatActionHandle& ActionHandle) const
{
	return ActionHandle.IsValid() && ActiveActionInstanceId == ActionHandle.Value && HasActiveHitWindow();
}

bool UCombatMeleeComponent::HasActiveHitWindow() const
{
	return ActiveActionInstanceId != 0 && ActiveHitWindowNotifyInstanceId != INDEX_NONE &&
	       ActiveDamageSegmentId > 0;
}

bool UCombatMeleeComponent::OwnsHitWindow(const FCombatActionHandle& ActionHandle,
	                                      const int32 NotifyInstanceId) const
{
	return IsHitWindowActive(ActionHandle) && NotifyInstanceId != INDEX_NONE &&
	       ActiveHitWindowNotifyInstanceId == NotifyInstanceId;
}

void UCombatMeleeComponent::ResetAction()
{
	ResetHitWindow();

	ActiveActionInstanceId = 0;
	ActiveDamageEffect = nullptr;
	ActiveMeleeTraceRadius = 0.0f;
	ActiveMeleeTraceBones.Reset();
	bActiveDamageCanTriggerPerfectDodge = false;
}

void UCombatMeleeComponent::ResetHitWindow()
{
	ActiveHitWindowNotifyInstanceId = INDEX_NONE;
	ActiveDamageSegmentId = 0;

	PreviousMeleeTraceLocations.Reset();
	HitActorsInCurrentWindow.Reset();
}

void UCombatMeleeComponent::SweepMeleeSegment(const FCombatActionHandle& ActionHandle,
	                                          const int32 NotifyInstanceId,
	                                          const FVector& Start,
	                                          const FVector& End)
{
	ACharacter* Character = CachedCharacter.Get();
	UWorld* World = GetWorld();

	if (!IsValid(Character) || !World || !OwnsHitWindow(ActionHandle, NotifyInstanceId))
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
		if (!OwnsHitWindow(ActionHandle, NotifyInstanceId))
		{
			return;
		}

		SubmitMeleeHit(ActionHandle, NotifyInstanceId, HitResult.GetActor());
	}
}

void UCombatMeleeComponent::SubmitMeleeHit(const FCombatActionHandle& ActionHandle,
	                                      const int32 NotifyInstanceId,
	                                      AActor* HitActor)
{
	ACharacter* Character = CachedCharacter.Get();

	if (!OwnsHitWindow(ActionHandle, NotifyInstanceId) || !IsValid(Character) ||
	    !IsValid(HitActor) || HitActor == Character)
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

	// Data flows from the action snapshot and collision result into a neutral
	// attempt. Target-side validation and GAS application happen downstream.
	FCombatDamageAttempt DamageAttempt;
	DamageAttempt.SourceActor = Character;
	DamageAttempt.DamageEffect = ActiveDamageEffect;
	DamageAttempt.AttackInstanceId = ActionHandle.Value;
	DamageAttempt.HitId = ActiveDamageSegmentId;
	DamageAttempt.HitTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	DamageAttempt.bCanTriggerPerfectDodge = bActiveDamageCanTriggerPerfectDodge;

	DamageComponent->ApplyDamageAttempt(DamageAttempt);
}
