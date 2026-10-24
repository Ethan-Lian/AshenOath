#include "Animation/AnimNotify_CombatVocal.h"

#include "AbilitySystem/Ability/AshenOathDodgeAbility.h"
#include "AbilitySystem/Ability/AshenOathMeleeAttackAbility.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Animation/ActiveMontageInstanceScope.h"
#include "Components/SkeletalMeshComponent.h"

void UAnimNotify_CombatVocal::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	if (EventReference.GetSourceObject() == Animation)
	{
		const UE::Anim::FAnimNotifyMontageInstanceContext* Context =
			EventReference.GetContextData<UE::Anim::FAnimNotifyMontageInstanceContext>();
		RequestVocal(MeshComp, Animation, Context ? Context->MontageInstanceID : INDEX_NONE);
	}
	Super::Notify(MeshComp, Animation, EventReference);
}

void UAnimNotify_CombatVocal::BranchingPointNotify(FBranchingPointNotifyPayload& Payload)
{
	// UAnimNotify's default branching handler calls Notify without montage context.
	RequestVocal(Payload.SkelMeshComponent, Payload.SequenceAsset, Payload.MontageInstanceID);
}

void UAnimNotify_CombatVocal::RequestVocal(
	USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const int32 MontageInstanceId) const
{
	AActor* Owner = IsValid(MeshComp) ? MeshComp->GetOwner() : nullptr;
	IAbilitySystemInterface* AbilityOwner = Cast<IAbilitySystemInterface>(Owner);
	UAbilitySystemComponent* ASC = AbilityOwner ? AbilityOwner->GetAbilitySystemComponent() : nullptr;
	if (!ASC || MontageInstanceId == INDEX_NONE)
	{
		return;
	}

	for (FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (!Spec.IsActive())
		{
			continue;
		}
		if (VocalType == EAshenOathVocalType::Dodge)
		{
			if (UAshenOathDodgeAbility* Ability = Cast<UAshenOathDodgeAbility>(Spec.GetPrimaryInstance()))
			{
				Ability->RequestVocalFromAnimation(MeshComp, Animation, MontageInstanceId, Sound.Get());
				return;
			}
		}
		else if (UAshenOathMeleeAttackAbility* Ability = Cast<UAshenOathMeleeAttackAbility>(Spec.GetPrimaryInstance()))
		{
			Ability->RequestVocalFromAnimation(MeshComp, Animation, MontageInstanceId, VocalType, Sound.Get());
			return;
		}
	}
}

FString UAnimNotify_CombatVocal::GetNotifyName_Implementation() const
{
	return FString::Printf(TEXT("Combat Vocal: %s"), *UEnum::GetValueAsString(VocalType));
}
