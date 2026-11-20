#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Feedback/AshenOathVocalTypes.h"
#include "AnimNotify_CombatVocal.generated.h"

/** An authored vocal opportunity; the owning action and feedback component decide playback. */
UCLASS(meta = (DisplayName = "Combat Vocal"))
class ASHENOATH_API UAnimNotify_CombatVocal : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference
	) override;
	virtual void BranchingPointNotify(FBranchingPointNotifyPayload& BranchingPointPayload) override;
	virtual FString GetNotifyName_Implementation() const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vocal")
	EAshenOathVocalType VocalType = EAshenOathVocalType::LightAttack;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vocal")
	TObjectPtr<USoundBase> Sound;

private:
	void RequestVocal(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, int32 MontageInstanceId) const;
};
