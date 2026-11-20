#include "AbilitySystem/Tasks/AshenOathAbilityTask_ComboTargetAssist.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystem/Data/AshenOathComboAttackData.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifyState_CombatHitWindow.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayTags/AshenOathGameplayTags.h"
#include "Targeting/CombatTargetingComponent.h"
#include "Tasks/AbilityTask_ApplyCombatMovement.h"

UAshenOathAbilityTask_ComboTargetAssist::UAshenOathAbilityTask_ComboTargetAssist(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTickingTask = true;
}

UAshenOathAbilityTask_ComboTargetAssist* UAshenOathAbilityTask_ComboTargetAssist::AssistComboTarget(
	UGameplayAbility* OwningAbility,
	const UAshenOathComboAttackData* ActionData,
	const int32 MontageInstanceId)
{
	UAshenOathAbilityTask_ComboTargetAssist* Task =
		NewAbilityTask<UAshenOathAbilityTask_ComboTargetAssist>(OwningAbility, TEXT("ComboTargetAssist"));
	Task->Data = ActionData;
	Task->ActiveMontageInstanceId = MontageInstanceId;
	return Task;
}

void UAshenOathAbilityTask_ComboTargetAssist::Activate()
{
	ACharacter* Avatar = Cast<ACharacter>(GetAvatarActor());
	if (!Avatar || !Data.IsValid() || !Data->Montage || Data->ComboSections.IsEmpty())
	{
		EndTask();
		return;
	}

	Character = Avatar;
	AnimInstance = Avatar->GetMesh()->GetAnimInstance();
	Targeting = Avatar->FindComponentByClass<UCombatTargetingComponent>();
	if (!AnimInstance.IsValid() || !Targeting.IsValid())
	{
		EndTask();
		return;
	}

	SetWaitingOnAvatar();
	TargetChangedHandle = Targeting->OnTargetChanged().AddUObject(this,
		&UAshenOathAbilityTask_ComboTargetAssist::HandleTargetChanged);
	TickTask(0.0f);
}

void UAshenOathAbilityTask_ComboTargetAssist::TickTask(const float DeltaTime)
{
	Super::TickTask(DeltaTime);
	if (IsFinished())
	{
		return;
	}
	const UAshenOathComboAttackData* ActionData = Data.Get();
	UAnimInstance* Anim = AnimInstance.Get();
	FAnimMontageInstance* Instance = Anim ? Anim->GetMontageInstanceForID(ActiveMontageInstanceId) : nullptr;
	if (!Character.IsValid() || !Targeting.IsValid() || !ActionData || !Instance ||
		Instance->Montage != ActionData->Montage || !Instance->IsActive())
	{
		EndTask();
		return;
	}

	const float Position = Instance->GetPosition();
	const int32 SectionIndex = ActionData->Montage->GetSectionIndexFromPosition(Position);
	if (SectionIndex != CurrentSectionIndex)
	{
		StopSectionAssist();
		if (IsFinished())
		{
			return;
		}
		CurrentSectionIndex = SectionIndex;
		BeginSectionAssist(SectionIndex, Position, Instance->GetPlayRate());
		if (IsFinished())
		{
			return;
		}
	}

	if (!bSectionAssistActive)
	{
		return;
	}
	AActor* Target = AssistTarget.Get();
	if (Position >= WindupEndPosition || !IsTargetEligible(Target))
	{
		StopSectionAssist();
		return;
	}

	if (!bFirstSection)
	{
		ACharacter* Avatar = Character.Get();
		const float DesiredYaw = (Target->GetActorLocation() - Avatar->GetActorLocation()).Rotation().Yaw;
		const float Yaw = FMath::FixedTurn(Avatar->GetActorRotation().Yaw, DesiredYaw,
			ActionData->FinisherTurnRate * DeltaTime);
		Avatar->SetActorRotation(FRotator(0.0f, Yaw, 0.0f));
	}
	else if (MovementTask)
	{
		float TargetRadius = 0.0f;
		float TargetHalfHeight = 0.0f;
		Target->GetSimpleCollisionCylinder(TargetRadius, TargetHalfHeight);
		const float StopDistance = Character->GetCapsuleComponent()->GetScaledCapsuleRadius() +
			TargetRadius + ActionData->ApproachStopGap;
		if (FVector::DistSquared2D(Character->GetActorLocation(), Target->GetActorLocation()) <= FMath::Square(StopDistance))
		{
			StopSectionAssist();
		}
		else if (DeltaTime > 0.0f && MovementTask->GetState() == EGameplayTaskState::AwaitingActivation)
		{
			// Register this task before movement, so target and timeline checks run first.
			MovementTask->ReadyForActivation();
		}
	}
}

void UAshenOathAbilityTask_ComboTargetAssist::BeginSectionAssist(
	const int32 SectionIndex, const float MontagePosition, const float PlayRate)
{
	const UAshenOathComboAttackData* ActionData = Data.Get();
	UAnimMontage* Montage = ActionData->Montage;
	const FName Section = Montage->GetSectionName(SectionIndex);
	bFirstSection = Section == ActionData->ComboSections[0];
	const bool bFinisher = ActionData->ComboSections.Num() > 1 && Section == ActionData->ComboSections.Last();
	const float EffectivePlayRate = PlayRate * Montage->RateScale;
	if ((!bFirstSection && !bFinisher) || EffectivePlayRate <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	float SectionStart = 0.0f;
	float SectionEnd = 0.0f;
	Montage->GetSectionStartAndEndTime(SectionIndex, SectionStart, SectionEnd);
	WindupEndPosition = SectionEnd;
	bool bFoundHitWindow = false;
	for (const FAnimNotifyEvent& Event : Montage->Notifies)
	{
		if (Event.NotifyStateClass && Event.NotifyStateClass->IsA<UAnimNotifyState_CombatHitWindow>() &&
			Event.GetTriggerTime() >= SectionStart && Event.GetTriggerTime() < SectionEnd)
		{
			WindupEndPosition = FMath::Min(WindupEndPosition, Event.GetTriggerTime());
			bFoundHitWindow = true;
		}
	}
	AActor* Target = Targeting->GetTarget();
	if (!bFoundHitWindow || MontagePosition >= WindupEndPosition || !IsTargetEligible(Target))
	{
		return;
	}

	AssistTarget = Target;
	bSectionAssistActive = true;
	if (!bFirstSection)
	{
		return;
	}

	ACharacter* Avatar = Character.Get();
	const FVector Offset = Target->GetActorLocation() - Avatar->GetActorLocation();
	const FVector Direction = Offset.GetSafeNormal2D();
	Avatar->SetActorRotation(FRotator(0.0f, Direction.Rotation().Yaw, 0.0f));
	if (IsFinished() || !Avatar->GetCharacterMovement()->IsMovingOnGround())
	{
		return;
	}

	float TargetRadius = 0.0f;
	float TargetHalfHeight = 0.0f;
	Target->GetSimpleCollisionCylinder(TargetRadius, TargetHalfHeight);
	const float StopDistance = Avatar->GetCapsuleComponent()->GetScaledCapsuleRadius() +
		TargetRadius + ActionData->ApproachStopGap;
	const float TravelDistance = FMath::Min(Offset.Size2D() - StopDistance, ActionData->ApproachMaxTravelDistance);
	if (TravelDistance <= KINDA_SMALL_NUMBER || ActionData->ApproachDuration <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float Duration = FMath::Min(ActionData->ApproachDuration,
		(WindupEndPosition - MontagePosition) / EffectivePlayRate);
	UAbilityTask_ApplyCombatMovement* NewMovement = UAbilityTask_ApplyCombatMovement::ApplyCombatMovement(
		Ability, TEXT("ComboFirstHitApproach"), Direction, TravelDistance, 0.0f, Duration, GetWorld()->GetTimeSeconds());
	MovementTask = NewMovement;
}

bool UAshenOathAbilityTask_ComboTargetAssist::IsTargetEligible(const AActor* Target) const
{
	const ACharacter* Avatar = Character.Get();
	const UAshenOathComboAttackData* ActionData = Data.Get();
	if (!IsValid(Target) || Target->IsActorBeingDestroyed() || Targeting->GetTarget() != Target)
	{
		return false;
	}
	const IAbilitySystemInterface* TargetInterface = Cast<IAbilitySystemInterface>(Target);
	const UAbilitySystemComponent* TargetASC = TargetInterface ? TargetInterface->GetAbilitySystemComponent() : nullptr;
	if (TargetASC && TargetASC->HasMatchingGameplayTag(AshenOathGameplayTags::State_Dead))
	{
		return false;
	}

	const FVector Offset = Target->GetActorLocation() - Avatar->GetActorLocation();
	if (Offset.SizeSquared2D() <= SMALL_NUMBER ||
		Offset.SizeSquared2D() > FMath::Square(ActionData->AssistMaxDistance) ||
		FMath::Abs(Offset.Z) > ActionData->AssistMaxHeightDifference ||
		FVector::DotProduct(Avatar->GetActorForwardVector().GetSafeNormal2D(), Offset.GetSafeNormal2D()) <
			FMath::Cos(FMath::DegreesToRadians(ActionData->AssistMaxAngleDegrees)))
	{
		return false;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ComboTargetAssist), false, Avatar);
	Params.AddIgnoredActor(Target);
	return !GetWorld()->LineTraceTestByChannel(Avatar->GetActorLocation(), Target->GetActorLocation(), ECC_Visibility, Params);
}

void UAshenOathAbilityTask_ComboTargetAssist::StopSectionAssist()
{
	bSectionAssistActive = false;
	AssistTarget.Reset();
	UAbilityTask_ApplyCombatMovement* Movement = MovementTask;
	MovementTask = nullptr;
	if (Movement)
	{
		Movement->EndTask();
	}
}

void UAshenOathAbilityTask_ComboTargetAssist::OnDestroy(const bool bInOwnerFinished)
{
	if (UCombatTargetingComponent* TargetComponent = Targeting.Get())
	{
		TargetComponent->OnTargetChanged().Remove(TargetChangedHandle);
	}
	TargetChangedHandle.Reset();
	StopSectionAssist();
	Super::OnDestroy(bInOwnerFinished);
}

void UAshenOathAbilityTask_ComboTargetAssist::HandleTargetChanged(AActor* PreviousTarget, AActor* NewTarget)
{
	if (bSectionAssistActive && NewTarget != AssistTarget.Get())
	{
		StopSectionAssist();
	}
}
