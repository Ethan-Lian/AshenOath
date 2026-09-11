#include "Animation/AnimNotifyState_CombatHitWindow.h"

#include "Actions/CombatActionComponent.h"

void UAnimNotifyState_CombatHitWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
                                                   float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	if (UCombatActionComponent* CombatActionComponent = ResolveCombatActionComponent(MeshComp))
	{
		CombatActionComponent->BeginMeleeHitWindow(EventReference.GetNotifyInstanceID(), HitId);
	}

	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
}

void UAnimNotifyState_CombatHitWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
                                                 const FAnimNotifyEventReference& EventReference)
{
	if (UCombatActionComponent* CombatActionComponent = ResolveCombatActionComponent(MeshComp))
	{
		CombatActionComponent->EndMeleeHitWindow(EventReference.GetNotifyInstanceID());
	}

	Super::NotifyEnd(MeshComp, Animation, EventReference);
}

FString UAnimNotifyState_CombatHitWindow::GetNotifyName_Implementation() const
{
	return TEXT("Combat Hit Window");
}

void UAnimNotifyState_CombatHitWindow::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
                                                  float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	if (UCombatActionComponent* CombatActionComponent = ResolveCombatActionComponent(MeshComp))
	{
		CombatActionComponent->TickMeleeHitWindow();
	}

	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);
}

UCombatActionComponent* UAnimNotifyState_CombatHitWindow::ResolveCombatActionComponent(USkeletalMeshComponent* MeshComponent)
{
	AActor* Owner = IsValid(MeshComponent) ? MeshComponent->GetOwner() : nullptr;

	return IsValid(Owner) ? Owner->FindComponentByClass<UCombatActionComponent>() : nullptr;
}
