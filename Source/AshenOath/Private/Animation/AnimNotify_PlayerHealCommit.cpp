#include "Animation/AnimNotify_PlayerHealCommit.h"

#include "AbilitySystem/Ability/AshenOathHealAbility.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Animation/ActiveMontageInstanceScope.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameplayAbilitySpec.h"

namespace AshenOathHealNotify
{
	UAshenOathHealAbility* FindActiveAbility(USkeletalMeshComponent* MeshComponent)
	{
		AActor* Owner = IsValid(MeshComponent) ? MeshComponent->GetOwner() : nullptr;
		IAbilitySystemInterface* AbilitySystemOwner = IsValid(Owner)
			? Cast<IAbilitySystemInterface>(Owner)
			: nullptr;
		UAbilitySystemComponent* AbilitySystem = AbilitySystemOwner
			? AbilitySystemOwner->GetAbilitySystemComponent()
			: nullptr;

		if (!AbilitySystem)
		{
			return nullptr;
		}

		for (FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
		{
			if (Spec.IsActive())
			{
				UAshenOathHealAbility* Ability =
					Cast<UAshenOathHealAbility>(Spec.GetPrimaryInstance());
				if (Ability && Ability->IsActive())
				{
					return Ability;
				}
			}
		}

		return nullptr;
	}
}

void UAnimNotify_PlayerHealCommit::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	if (EventReference.GetSourceObject() == Animation)
	{
		const UE::Anim::FAnimNotifyMontageInstanceContext* MontageContext =
			EventReference.GetContextData<UE::Anim::FAnimNotifyMontageInstanceContext>();
		if (UAshenOathHealAbility* Ability =
			AshenOathHealNotify::FindActiveAbility(MeshComp))
		{
			Ability->TryCommitHealFromAnimation(
				MeshComp,
				Animation,
				MontageContext ? MontageContext->MontageInstanceID : INDEX_NONE
			);
		}
	}

	Super::Notify(MeshComp, Animation, EventReference);
}

void UAnimNotify_PlayerHealCommit::BranchingPointNotify(
	FBranchingPointNotifyPayload& BranchingPointPayload)
{
	if (UAshenOathHealAbility* Ability = AshenOathHealNotify::FindActiveAbility(
		BranchingPointPayload.SkelMeshComponent))
	{
		Ability->TryCommitHealFromAnimation(
			BranchingPointPayload.SkelMeshComponent,
			BranchingPointPayload.SequenceAsset,
			BranchingPointPayload.MontageInstanceID
		);
	}

	Super::BranchingPointNotify(BranchingPointPayload);
}

FString UAnimNotify_PlayerHealCommit::GetNotifyName_Implementation() const
{
	return TEXT("Player Heal Commit");
}
