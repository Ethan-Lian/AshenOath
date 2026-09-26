#include "Actions/CombatMeleeComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
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
	ResetSession();
	CachedCharacter.Reset();

	Super::EndPlay(EndPlayReason);
}

bool UCombatMeleeComponent::CanStartSession(
	TSubclassOf<UGameplayEffect> DamageEffect,
	const float TraceRadius,
	const TArray<FName>& TraceBones,
	const USkeletalMeshComponent* SourceMesh,
	const UAnimInstance* SourceAnimInstance) const
{
	if (ActiveSessionInstanceId != 0 ||
		!DamageEffect ||
		TraceRadius <= KINDA_SMALL_NUMBER ||
		TraceBones.Num() < 2)
	{
		return false;
	}

	const ACharacter* Character = CachedCharacter.IsValid()
		? CachedCharacter.Get()
		: Cast<ACharacter>(GetOwner());

	if (!IsValid(Character) ||
		Character->IsActorBeingDestroyed() ||
		!IsValid(SourceMesh) ||
		SourceMesh != Character->GetMesh() ||
		!IsValid(SourceAnimInstance) ||
		SourceAnimInstance->GetSkelMeshComponent() != SourceMesh)
	{
		return false;
	}

	for (const FName TracePointName : TraceBones)
	{
		if (TracePointName.IsNone() ||
			!SourceMesh->DoesSocketExist(TracePointName))
		{
			return false;
		}
	}

	return true;
}

FCombatMeleeSessionHandle UCombatMeleeComponent::BeginSession(
	const FCombatMeleeSessionRequest& Request)
{
	FCombatMeleeSessionHandle SessionHandle;

	// A second caller must not erase the state owned by the current caller.
	if (!CanStartSession(
			Request.DamageEffect,
			Request.TraceRadius,
			Request.TraceBones,
			Request.SourceMesh,
			Request.SourceAnimInstance) ||
		!IsValid(Request.SourceMontage) ||
		Request.MontageInstanceId == INDEX_NONE)
	{
		return SessionHandle;
	}

	const FAnimMontageInstance* MontageInstance =
		Request.SourceAnimInstance->GetMontageInstanceForID(Request.MontageInstanceId);

	if (!MontageInstance ||
		MontageInstance->Montage != Request.SourceMontage ||
		!MontageInstance->IsActive())
	{
		return SessionHandle;
	}

	ResetHitWindow();

	SessionHandle.Value = AllocateSessionInstanceId();

	ActiveSessionInstanceId = SessionHandle.Value;
	ActiveMontageInstanceId = Request.MontageInstanceId;
	ActiveSourceMesh = Request.SourceMesh;
	ActiveSourceAnimInstance = Request.SourceAnimInstance;
	ActiveSourceMontage = Request.SourceMontage;
	ActiveDamageEffect = Request.DamageEffect;
	ActiveSetByCallerMagnitudeTag = Request.SetByCallerMagnitudeTag;
	ActiveSetByCallerMagnitude = Request.SetByCallerMagnitude;
	bActiveDamageCanTriggerPerfectDodge = Request.bCanTriggerPerfectDodge;
	ActiveMeleeTraceRadius = Request.TraceRadius;
	ActiveMeleeTraceBones = Request.TraceBones;

	return SessionHandle;
}

void UCombatMeleeComponent::EndSession(const FCombatMeleeSessionHandle& SessionHandle)
{
	if (!IsSessionActive(SessionHandle))
	{
		return;
	}

	ResetSession();
}

void UCombatMeleeComponent::ResetCombatState()
{
	ResetSession();
}

bool UCombatMeleeComponent::IsSessionActive(
	const FCombatMeleeSessionHandle& SessionHandle) const
{
	return SessionHandle.IsValid() &&
		ActiveSessionInstanceId == SessionHandle.Value;
}

bool UCombatMeleeComponent::HasActiveSession() const
{
	return ActiveSessionInstanceId != 0;
}

void UCombatMeleeComponent::BeginHitWindowFromAnimation(
	USkeletalMeshComponent* MeshComponent,
	UAnimSequenceBase* Animation,
	const int32 MontageInstanceId,
	const int32 NotifyInstanceId)
{
	if (!IsAnimationSignalOwned(
			MeshComponent,
			Animation,
			MontageInstanceId,
			true))
	{
		return;
	}

	BeginHitWindow(GetActiveSessionHandle(), NotifyInstanceId);
}

void UCombatMeleeComponent::EndHitWindowFromAnimation(
	USkeletalMeshComponent* MeshComponent,
	UAnimSequenceBase* Animation,
	const int32 MontageInstanceId,
	const int32 NotifyInstanceId)
{
	if (!IsAnimationSignalOwned(
			MeshComponent,
			Animation,
			MontageInstanceId,
			false))
	{
		return;
	}

	EndHitWindow(GetActiveSessionHandle(), NotifyInstanceId);
}

void UCombatMeleeComponent::TickHitWindowFromAnimation(
	USkeletalMeshComponent* MeshComponent,
	UAnimSequenceBase* Animation,
	const int32 MontageInstanceId,
	const int32 NotifyInstanceId)
{
	if (!IsAnimationSignalOwned(
			MeshComponent,
			Animation,
			MontageInstanceId,
			true))
	{
		return;
	}

	TickHitWindow(GetActiveSessionHandle(), NotifyInstanceId);
}

void UCombatMeleeComponent::BeginHitWindow(
	const FCombatMeleeSessionHandle& SessionHandle,
	const int32 NotifyInstanceId)
{
	if (!IsSessionActive(SessionHandle) ||
		HasActiveHitWindow() ||
		NotifyInstanceId == INDEX_NONE)
	{
		return;
	}

	USkeletalMeshComponent* MeshComponent = ActiveSourceMesh.Get();

	if (!IsValid(MeshComponent) ||
		!ActiveDamageEffect ||
		ActiveMeleeTraceRadius <= KINDA_SMALL_NUMBER ||
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

		PreviousMeleeTraceLocations[Index] =
			MeshComponent->GetSocketLocation(TracePointName);
	}

	ActiveHitWindowNotifyInstanceId = NotifyInstanceId;
	HitActorsInCurrentWindow.Reset();
}

void UCombatMeleeComponent::EndHitWindow(
	const FCombatMeleeSessionHandle& SessionHandle,
	const int32 NotifyInstanceId)
{
	if (!OwnsHitWindow(SessionHandle, NotifyInstanceId))
	{
		return;
	}

	ResetHitWindow();
}

void UCombatMeleeComponent::TickHitWindow(
	const FCombatMeleeSessionHandle& SessionHandle,
	const int32 NotifyInstanceId)
{
	if (!OwnsHitWindow(SessionHandle, NotifyInstanceId))
	{
		return;
	}

	USkeletalMeshComponent* MeshComponent = ActiveSourceMesh.Get();

	if (!IsValid(MeshComponent) ||
		PreviousMeleeTraceLocations.Num() != ActiveMeleeTraceBones.Num())
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
		SweepMeleeSegment(
			SessionHandle,
			NotifyInstanceId,
			PreviousMeleeTraceLocations[Index],
			CurrentTraceLocations[Index]
		);

		// Applying damage may synchronously re-enter combat code and end the session.
		if (!OwnsHitWindow(SessionHandle, NotifyInstanceId))
		{
			return;
		}
	}

	// Sweep between adjacent trace points at the weapon's current pose.
	// This fills the gaps between individual trace points so the weapon body
	// can hit targets, not just the sampled sockets/bones.
	for (int32 Index = 1; Index < CurrentTraceLocations.Num(); ++Index)
	{
		SweepMeleeSegment(
			SessionHandle,
			NotifyInstanceId,
			CurrentTraceLocations[Index - 1],
			CurrentTraceLocations[Index]
		);

		if (!OwnsHitWindow(SessionHandle, NotifyInstanceId))
		{
			return;
		}
	}

	PreviousMeleeTraceLocations = MoveTemp(CurrentTraceLocations);
}

bool UCombatMeleeComponent::IsHitWindowActive(
	const FCombatMeleeSessionHandle& SessionHandle) const
{
	return IsSessionActive(SessionHandle) && HasActiveHitWindow();
}

int32 UCombatMeleeComponent::AllocateSessionInstanceId()
{
	const int32 AllocatedId = NextSessionInstanceId;

	NextSessionInstanceId = NextSessionInstanceId == MAX_int32
		? 1
		: NextSessionInstanceId + 1;

	return AllocatedId;
}

void UCombatMeleeComponent::ResetSession()
{
	ResetHitWindow();

	// Revoke identity first. Any re-entrant animation callback now fails before
	// the external references and immutable snapshot are released.
	ActiveSessionInstanceId = 0;
	ActiveMontageInstanceId = INDEX_NONE;
	ActiveSourceMesh.Reset();
	ActiveSourceAnimInstance.Reset();
	ActiveSourceMontage.Reset();
	ActiveDamageEffect = nullptr;
	ActiveSetByCallerMagnitudeTag = FGameplayTag();
	ActiveSetByCallerMagnitude = 0.0f;
	bActiveDamageCanTriggerPerfectDodge = false;
	ActiveMeleeTraceRadius = 0.0f;
	ActiveMeleeTraceBones.Reset();
}

void UCombatMeleeComponent::ResetHitWindow()
{
	ActiveHitWindowNotifyInstanceId = INDEX_NONE;
	PreviousMeleeTraceLocations.Reset();
	HitActorsInCurrentWindow.Reset();
}

void UCombatMeleeComponent::SweepMeleeSegment(
	const FCombatMeleeSessionHandle& SessionHandle,
	const int32 NotifyInstanceId,
	const FVector& Start,
	const FVector& End)
{
	ACharacter* Character = CachedCharacter.Get();
	UWorld* World = GetWorld();

	if (!IsValid(Character) ||
		!World ||
		!OwnsHitWindow(SessionHandle, NotifyInstanceId))
	{
		return;
	}

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(AshenOathMeleeSweep),
		false,
		Character
	);
	const FCollisionObjectQueryParams ObjectQueryParams(ECC_Pawn);

	TArray<FHitResult> HitResults;

	World->SweepMultiByObjectType(
		HitResults,
		Start,
		End,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(ActiveMeleeTraceRadius),
		QueryParams
	);

	for (const FHitResult& HitResult : HitResults)
	{
		if (!OwnsHitWindow(SessionHandle, NotifyInstanceId))
		{
			return;
		}

		SubmitMeleeHit(SessionHandle, NotifyInstanceId, HitResult.GetActor());
	}
}

void UCombatMeleeComponent::SubmitMeleeHit(
	const FCombatMeleeSessionHandle& SessionHandle,
	const int32 NotifyInstanceId,
	AActor* HitActor)
{
	ACharacter* Character = CachedCharacter.Get();

	if (!OwnsHitWindow(SessionHandle, NotifyInstanceId) ||
		!IsValid(Character) ||
		!IsValid(HitActor) ||
		HitActor == Character)
	{
		return;
	}

	const TWeakObjectPtr<AActor> HitActorKey(HitActor);

	if (HitActorsInCurrentWindow.Contains(HitActorKey))
	{
		return;
	}

	UCombatDamageComponent* DamageComponent =
		HitActor->FindComponentByClass<UCombatDamageComponent>();

	if (!DamageComponent)
	{
		return;
	}

	// Claim the actor before applying damage because GAS callbacks can
	// synchronously re-enter combat code and submit the same target again.
	HitActorsInCurrentWindow.Add(HitActorKey);

	FCombatDamageAttempt DamageAttempt;
	DamageAttempt.SourceActor = Character;
	DamageAttempt.DamageEffect = ActiveDamageEffect;
	DamageAttempt.HitTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	DamageAttempt.bCanTriggerPerfectDodge = bActiveDamageCanTriggerPerfectDodge;
	DamageAttempt.SetByCallerMagnitudeTag = ActiveSetByCallerMagnitudeTag;
	DamageAttempt.SetByCallerMagnitude = ActiveSetByCallerMagnitude;

	DamageComponent->ApplyDamageAttempt(DamageAttempt);
}

bool UCombatMeleeComponent::HasActiveHitWindow() const
{
	return ActiveSessionInstanceId != 0 &&
		ActiveHitWindowNotifyInstanceId != INDEX_NONE;
}

bool UCombatMeleeComponent::OwnsHitWindow(
	const FCombatMeleeSessionHandle& SessionHandle,
	const int32 NotifyInstanceId) const
{
	return IsHitWindowActive(SessionHandle) &&
		NotifyInstanceId != INDEX_NONE &&
		ActiveHitWindowNotifyInstanceId == NotifyInstanceId;
}

bool UCombatMeleeComponent::IsAnimationSignalOwned(
	const USkeletalMeshComponent* MeshComponent,
	const UAnimSequenceBase* Animation,
	const int32 MontageInstanceId,
	const bool bRequireActiveMontage) const
{
	if (ActiveSessionInstanceId == 0 ||
		MontageInstanceId == INDEX_NONE ||
		MontageInstanceId != ActiveMontageInstanceId ||
		!IsValid(MeshComponent) ||
		!IsValid(Animation) ||
		MeshComponent != ActiveSourceMesh.Get())
	{
		return false;
	}

	UAnimInstance* AnimInstance = ActiveSourceAnimInstance.Get();

	if (!IsValid(AnimInstance) ||
		AnimInstance->GetSkelMeshComponent() != MeshComponent)
	{
		return false;
	}

	FAnimMontageInstance* MontageInstance =
		AnimInstance->GetMontageInstanceForID(MontageInstanceId);

	// NotifyEnd can be dispatched while its Montage instance is being removed.
	// The already-matched Mesh, AnimInstance and instance ID are sufficient to
	// close this session's own window, but opening/ticking requires a live owner.
	if (!MontageInstance)
	{
		return !bRequireActiveMontage;
	}

	if (MontageInstance->Montage != ActiveSourceMontage.Get())
	{
		return false;
	}

	return !bRequireActiveMontage || MontageInstance->IsActive();
}

FCombatMeleeSessionHandle UCombatMeleeComponent::GetActiveSessionHandle() const
{
	FCombatMeleeSessionHandle SessionHandle;
	SessionHandle.Value = ActiveSessionInstanceId;
	return SessionHandle;
}
