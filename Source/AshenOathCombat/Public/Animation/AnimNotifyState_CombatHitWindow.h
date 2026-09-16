#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_CombatHitWindow.generated.h"

class UCombatMeleeComponent;

/**
 * Animation-timeline trigger for a melee hit window.
 *
 * This notify state does not own runtime combat state. Notify assets may be
 * reused or executed multiple times, so hit tracking and window state remain
 * owned by UCombatMeleeComponent. Every call carries the source Montage instance
 * identity and is rejected unless it belongs to the active melee session.
 *
 * NotifyBegin opens the window, NotifyTick advances melee tracing, and
 * NotifyEnd closes the exact notify instance that opened it.
 */
UCLASS(meta = (DisplayName = "Combat Hit Window"))
class ASHENOATHCOMBAT_API UAnimNotifyState_CombatHitWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UAnimNotifyState_CombatHitWindow();
	virtual void PostLoad() override;

	virtual void NotifyBegin(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		float TotalDuration,
		const FAnimNotifyEventReference& EventReference
	) override;

	virtual void NotifyEnd(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference
	) override;

	virtual void NotifyTick(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		float FrameDeltaTime,
		const FAnimNotifyEventReference& EventReference
	) override;

	// Branching-point callbacks do not populate FAnimNotifyEventReference in UE
	// 5.8, so forward the playback identity carried by their payload explicitly.
	virtual void BranchingPointNotifyBegin(FBranchingPointNotifyPayload& BranchingPointPayload) override;
	virtual void BranchingPointNotifyTick(
		FBranchingPointNotifyPayload& BranchingPointPayload,
		float FrameDeltaTime
	) override;
	virtual void BranchingPointNotifyEnd(FBranchingPointNotifyPayload& BranchingPointPayload) override;

	virtual FString GetNotifyName_Implementation() const override;

private:
	static UCombatMeleeComponent* ResolveCombatMeleeComponent(
		USkeletalMeshComponent* MeshComponent
	);

	static int32 ResolveMontageInstanceId(
		const FAnimNotifyEventReference& EventReference
	);

	static int32 ResolveBranchingNotifyInstanceId(
		const FBranchingPointNotifyPayload& BranchingPointPayload
	);
};
