#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_CombatHitWindow.generated.h"

class UCombatActionComponent;


/**
 * Animation-timeline trigger for a melee hit window.
 *
 * This notify state does not own runtime combat state. Notify assets may be
 * reused or executed multiple times, so hit tracking and window state remain
 * owned by UCombatActionComponent.
 *
 * NotifyBegin opens the window, NotifyTick advances melee tracing, and
 * NotifyEnd closes the exact notify instance that opened it.
 */
UCLASS(meta = (DisplayName = "Combat Hit Window"))
class ASHENOATHCOMBAT_API UAnimNotifyState_CombatHitWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	// open the attack windows for this notify instance.
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration,
	                         const FAnimNotifyEventReference& EventReference) override;

	// close the attack windows for this notifyinstance.
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	                       const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

	// Advances melee sweep detection while the hit window is active.
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime,
	                        const FAnimNotifyEventReference& EventReference) override;

	// Identifies the damage segment represented by this hit window.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Hit Window", meta = (ClampMin = "1"))
	int32 HitId = 1;

private:
	static UCombatActionComponent* ResolveCombatActionComponent(USkeletalMeshComponent* MeshComponent);
};
