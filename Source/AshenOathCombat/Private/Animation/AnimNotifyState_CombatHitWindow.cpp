#include "Animation/AnimNotifyState_CombatHitWindow.h"

#include "Actions/CombatMeleeComponent.h"
#include "Animation/ActiveMontageInstanceScope.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Components/SkeletalMeshComponent.h"

UAnimNotifyState_CombatHitWindow::UAnimNotifyState_CombatHitWindow()
{
	// UE otherwise merges the same notify state across concurrent plays. Keeping
	// their lifecycles separate is required for Montage-instance ownership checks.
	NotifyStateBehaviorFlags |=
		static_cast<uint8>(EAnimNotifyStateBehaviorFlags::NoMergeOnConcurrentPlay);
}

void UAnimNotifyState_CombatHitWindow::PostLoad()
{
	Super::PostLoad();

	// Existing Montage assets may have serialized the old zero value. Enforce the
	// ownership rule at runtime without requiring an asset resave for stage B.
	NotifyStateBehaviorFlags |=
		static_cast<uint8>(EAnimNotifyStateBehaviorFlags::NoMergeOnConcurrentPlay);
}

void UAnimNotifyState_CombatHitWindow::NotifyBegin(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	const int32 MontageInstanceId = ResolveMontageInstanceId(EventReference);

	if (EventReference.GetSourceObject() == Animation)
	{
		if (UCombatMeleeComponent* Melee = ResolveCombatMeleeComponent(MeshComp))
		{
			Melee->BeginHitWindowFromAnimation(
				MeshComp,
				Animation,
				MontageInstanceId,
				EventReference.GetNotifyInstanceID()
			);
		}
	}

	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
}

void UAnimNotifyState_CombatHitWindow::NotifyEnd(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	const int32 MontageInstanceId = ResolveMontageInstanceId(EventReference);

	if (EventReference.GetSourceObject() == Animation)
	{
		if (UCombatMeleeComponent* Melee = ResolveCombatMeleeComponent(MeshComp))
		{
			Melee->EndHitWindowFromAnimation(
				MeshComp,
				Animation,
				MontageInstanceId,
				EventReference.GetNotifyInstanceID()
			);
		}
	}

	Super::NotifyEnd(MeshComp, Animation, EventReference);
}

void UAnimNotifyState_CombatHitWindow::NotifyTick(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const float FrameDeltaTime,
	const FAnimNotifyEventReference& EventReference)
{
	const int32 MontageInstanceId = ResolveMontageInstanceId(EventReference);

	if (EventReference.GetSourceObject() == Animation)
	{
		if (UCombatMeleeComponent* Melee = ResolveCombatMeleeComponent(MeshComp))
		{
			Melee->TickHitWindowFromAnimation(
				MeshComp,
				Animation,
				MontageInstanceId,
				EventReference.GetNotifyInstanceID()
			);
		}
	}

	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);
}

void UAnimNotifyState_CombatHitWindow::BranchingPointNotifyBegin(
	FBranchingPointNotifyPayload& BranchingPointPayload)
{
	if (UCombatMeleeComponent* Melee =
		ResolveCombatMeleeComponent(BranchingPointPayload.SkelMeshComponent))
	{
		Melee->BeginHitWindowFromAnimation(
			BranchingPointPayload.SkelMeshComponent,
			BranchingPointPayload.SequenceAsset,
			BranchingPointPayload.MontageInstanceID,
			ResolveBranchingNotifyInstanceId(BranchingPointPayload)
		);
	}

	Super::BranchingPointNotifyBegin(BranchingPointPayload);
}

void UAnimNotifyState_CombatHitWindow::BranchingPointNotifyTick(
	FBranchingPointNotifyPayload& BranchingPointPayload,
	const float FrameDeltaTime)
{
	if (UCombatMeleeComponent* Melee =
		ResolveCombatMeleeComponent(BranchingPointPayload.SkelMeshComponent))
	{
		Melee->TickHitWindowFromAnimation(
			BranchingPointPayload.SkelMeshComponent,
			BranchingPointPayload.SequenceAsset,
			BranchingPointPayload.MontageInstanceID,
			ResolveBranchingNotifyInstanceId(BranchingPointPayload)
		);
	}

	Super::BranchingPointNotifyTick(BranchingPointPayload, FrameDeltaTime);
}

void UAnimNotifyState_CombatHitWindow::BranchingPointNotifyEnd(
	FBranchingPointNotifyPayload& BranchingPointPayload)
{
	if (UCombatMeleeComponent* Melee =
		ResolveCombatMeleeComponent(BranchingPointPayload.SkelMeshComponent))
	{
		Melee->EndHitWindowFromAnimation(
			BranchingPointPayload.SkelMeshComponent,
			BranchingPointPayload.SequenceAsset,
			BranchingPointPayload.MontageInstanceID,
			ResolveBranchingNotifyInstanceId(BranchingPointPayload)
		);
	}

	Super::BranchingPointNotifyEnd(BranchingPointPayload);
}

FString UAnimNotifyState_CombatHitWindow::GetNotifyName_Implementation() const
{
	return TEXT("Combat Hit Window");
}

UCombatMeleeComponent* UAnimNotifyState_CombatHitWindow::ResolveCombatMeleeComponent(
	USkeletalMeshComponent* MeshComponent)
{
	AActor* Owner = IsValid(MeshComponent) ? MeshComponent->GetOwner() : nullptr;

	return IsValid(Owner)
		? Owner->FindComponentByClass<UCombatMeleeComponent>()
		: nullptr;
}

int32 UAnimNotifyState_CombatHitWindow::ResolveMontageInstanceId(
	const FAnimNotifyEventReference& EventReference)
{
	const UE::Anim::FAnimNotifyMontageInstanceContext* MontageContext =
		EventReference.GetContextData<UE::Anim::FAnimNotifyMontageInstanceContext>();

	return MontageContext ? MontageContext->MontageInstanceID : INDEX_NONE;
}

int32 UAnimNotifyState_CombatHitWindow::ResolveBranchingNotifyInstanceId(
	const FBranchingPointNotifyPayload& BranchingPointPayload)
{
	// The FAnimNotifyEvent address is stable across begin/tick/end for this active
	// branching point. The Montage instance ID supplies the execution identity.
	return static_cast<int32>(PointerHash(BranchingPointPayload.NotifyEvent) & MAX_int32);
}
