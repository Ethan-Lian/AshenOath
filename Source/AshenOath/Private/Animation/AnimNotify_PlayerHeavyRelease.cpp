#include "Animation/AnimNotify_PlayerHeavyRelease.h"

#include "AbilitySystem/Ability/AshenOathHeavyAttackAbility.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Animation/ActiveMontageInstanceScope.h"
#include "Components/SkeletalMeshComponent.h"

void UAnimNotify_PlayerHeavyRelease::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	if (EventReference.GetSourceObject() == Animation)
	{
		const auto* Context = EventReference.GetContextData<UE::Anim::FAnimNotifyMontageInstanceContext>();
		RequestRelease(MeshComp, Animation, Context ? Context->MontageInstanceID : INDEX_NONE);
	}
	Super::Notify(MeshComp, Animation, EventReference);
}

void UAnimNotify_PlayerHeavyRelease::BranchingPointNotify(FBranchingPointNotifyPayload& Payload)
{
	RequestRelease(Payload.SkelMeshComponent, Payload.SequenceAsset, Payload.MontageInstanceID);
}

void UAnimNotify_PlayerHeavyRelease::RequestRelease(
	USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation, const int32 MontageInstanceId) const
{
	IAbilitySystemInterface* Owner = IsValid(Mesh) ? Cast<IAbilitySystemInterface>(Mesh->GetOwner()) : nullptr;
	UAbilitySystemComponent* ASC = Owner ? Owner->GetAbilitySystemComponent() : nullptr;
	if (!ASC || MontageInstanceId == INDEX_NONE)
	{
		return;
	}
	for (FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.IsActive())
		{
			if (auto* Ability = Cast<UAshenOathHeavyAttackAbility>(Spec.GetPrimaryInstance()))
			{
				Ability->RequestReleaseFeedbackFromAnimation(Mesh, Animation, MontageInstanceId);
				return;
			}
		}
	}
}

FString UAnimNotify_PlayerHeavyRelease::GetNotifyName_Implementation() const
{
	return TEXT("Player Heavy Release");
}
