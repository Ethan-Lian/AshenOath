#include "Animation/AnimNotifyState_PlayerComboWindow.h"

#include "AbilitySystem/Ability/AshenOathComboAttackAbility.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Animation/ActiveMontageInstanceScope.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameplayAbilitySpec.h"

UAnimNotifyState_PlayerComboWindow::UAnimNotifyState_PlayerComboWindow()
{
	NotifyStateBehaviorFlags |=
		static_cast<uint8>(EAnimNotifyStateBehaviorFlags::NoMergeOnConcurrentPlay);
}

void UAnimNotifyState_PlayerComboWindow::PostLoad()
{
	Super::PostLoad();
	NotifyStateBehaviorFlags |=
		static_cast<uint8>(EAnimNotifyStateBehaviorFlags::NoMergeOnConcurrentPlay);
}

void UAnimNotifyState_PlayerComboWindow::NotifyBegin(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	if (EventReference.GetSourceObject() == Animation)
	{
		if (UAshenOathComboAttackAbility* Ability =
			ResolveActiveComboAbility(MeshComp))
		{
			Ability->BeginComboWindowFromAnimation(
				MeshComp,
				Animation,
				ResolveMontageInstanceId(EventReference),
				EventReference.GetNotifyInstanceID()
			);
		}
	}

	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
}

void UAnimNotifyState_PlayerComboWindow::NotifyEnd(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	if (EventReference.GetSourceObject() == Animation)
	{
		if (UAshenOathComboAttackAbility* Ability =
			ResolveActiveComboAbility(MeshComp))
		{
			Ability->EndComboWindowFromAnimation(
				MeshComp,
				Animation,
				ResolveMontageInstanceId(EventReference),
				EventReference.GetNotifyInstanceID()
			);
		}
	}

	Super::NotifyEnd(MeshComp, Animation, EventReference);
}

void UAnimNotifyState_PlayerComboWindow::BranchingPointNotifyBegin(
	FBranchingPointNotifyPayload& BranchingPointPayload)
{
	if (UAshenOathComboAttackAbility* Ability = ResolveActiveComboAbility(
		BranchingPointPayload.SkelMeshComponent))
	{
		Ability->BeginComboWindowFromAnimation(
			BranchingPointPayload.SkelMeshComponent,
			BranchingPointPayload.SequenceAsset,
			BranchingPointPayload.MontageInstanceID,
			ResolveBranchingNotifyInstanceId(BranchingPointPayload)
		);
	}

	Super::BranchingPointNotifyBegin(BranchingPointPayload);
}

void UAnimNotifyState_PlayerComboWindow::BranchingPointNotifyEnd(
	FBranchingPointNotifyPayload& BranchingPointPayload)
{
	if (UAshenOathComboAttackAbility* Ability = ResolveActiveComboAbility(
		BranchingPointPayload.SkelMeshComponent))
	{
		Ability->EndComboWindowFromAnimation(
			BranchingPointPayload.SkelMeshComponent,
			BranchingPointPayload.SequenceAsset,
			BranchingPointPayload.MontageInstanceID,
			ResolveBranchingNotifyInstanceId(BranchingPointPayload)
		);
	}

	Super::BranchingPointNotifyEnd(BranchingPointPayload);
}

FString UAnimNotifyState_PlayerComboWindow::GetNotifyName_Implementation() const
{
	return TEXT("Player Combo Window");
}

UAshenOathComboAttackAbility*
UAnimNotifyState_PlayerComboWindow::ResolveActiveComboAbility(
	USkeletalMeshComponent* MeshComponent)
{
	AActor* Owner = IsValid(MeshComponent) ? MeshComponent->GetOwner() : nullptr;
	IAbilitySystemInterface* AbilitySystemOwner =
		IsValid(Owner) ? Cast<IAbilitySystemInterface>(Owner) : nullptr;
	UAbilitySystemComponent* AbilitySystem = AbilitySystemOwner
		? AbilitySystemOwner->GetAbilitySystemComponent()
		: nullptr;

	if (!AbilitySystem)
	{
		return nullptr;
	}

	for (FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
	{
		if (!Spec.IsActive())
		{
			continue;
		}

		UAshenOathComboAttackAbility* Ability =
			Cast<UAshenOathComboAttackAbility>(Spec.GetPrimaryInstance());

		if (Ability && Ability->IsActive())
		{
			return Ability;
		}
	}

	return nullptr;
}

int32 UAnimNotifyState_PlayerComboWindow::ResolveMontageInstanceId(
	const FAnimNotifyEventReference& EventReference)
{
	const UE::Anim::FAnimNotifyMontageInstanceContext* MontageContext =
		EventReference.GetContextData<UE::Anim::FAnimNotifyMontageInstanceContext>();

	return MontageContext ? MontageContext->MontageInstanceID : INDEX_NONE;
}

int32 UAnimNotifyState_PlayerComboWindow::ResolveBranchingNotifyInstanceId(
	const FBranchingPointNotifyPayload& BranchingPointPayload)
{
	return static_cast<int32>(
		PointerHash(BranchingPointPayload.NotifyEvent) & MAX_int32
	);
}
