#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_PlayerHeavyRelease.generated.h"

/** Authored release cue; the owning Heavy Ability validates the montage execution. */
UCLASS(meta = (DisplayName = "Player Heavy Release"))
class ASHENOATH_API UAnimNotify_PlayerHeavyRelease : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
	virtual void BranchingPointNotify(FBranchingPointNotifyPayload& Payload) override;
	virtual FString GetNotifyName_Implementation() const override;

private:
	void RequestRelease(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation, int32 MontageInstanceId) const;
};
