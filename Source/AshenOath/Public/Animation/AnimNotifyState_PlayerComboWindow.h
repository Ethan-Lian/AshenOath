#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_PlayerComboWindow.generated.h"

class UAshenOathComboAttackAbility;

/** Routes a source-identified Montage window to the active player combo Ability. */
UCLASS(meta = (DisplayName = "Player Combo Window"))
class ASHENOATH_API UAnimNotifyState_PlayerComboWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UAnimNotifyState_PlayerComboWindow();
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

	virtual void BranchingPointNotifyBegin(
		FBranchingPointNotifyPayload& BranchingPointPayload
	) override;

	virtual void BranchingPointNotifyEnd(
		FBranchingPointNotifyPayload& BranchingPointPayload
	) override;

	virtual FString GetNotifyName_Implementation() const override;

private:
	static UAshenOathComboAttackAbility* ResolveActiveComboAbility(
		USkeletalMeshComponent* MeshComponent
	);

	static int32 ResolveMontageInstanceId(
		const FAnimNotifyEventReference& EventReference
	);

	static int32 ResolveBranchingNotifyInstanceId(
		const FBranchingPointNotifyPayload& BranchingPointPayload
	);
};
