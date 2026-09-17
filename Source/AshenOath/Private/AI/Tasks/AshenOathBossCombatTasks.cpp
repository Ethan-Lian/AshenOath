#include "AI/Tasks/AshenOathBossCombatTasks.h"

#include "AIController.h"
#include "AITypes.h"
#include "Characters/AshenOathBossCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"
#include "StateTreeAsyncExecutionContext.h"
#include "StateTreeExecutionContext.h"

namespace
{
	AAshenOathBossCharacter* ResolveBoss(FStateTreeExecutionContext& Context)
	{
		if (AAshenOathBossCharacter* Boss = Cast<AAshenOathBossCharacter>(Context.GetOwner()))
		{
			return Boss;
		}

		const AAIController* AIController = Cast<AAIController>(Context.GetOwner());
		return AIController
			? Cast<AAshenOathBossCharacter>(AIController->GetPawn())
			: nullptr;
	}

	bool IsWithinRange(const AActor& Source, const AActor& Target, const float Range)
	{
		return FVector::DistSquared2D(Source.GetActorLocation(), Target.GetActorLocation()) <=
			FMath::Square(FMath::Max(0.0f, Range));
	}

	void FaceTarget(AActor& Source, const AActor& Target)
	{
		const FVector ToTarget = Target.GetActorLocation() - Source.GetActorLocation();
		if (!ToTarget.IsNearlyZero())
		{
			Source.SetActorRotation(FRotator(0.0f, ToTarget.Rotation().Yaw, 0.0f));
		}
	}
}

EStateTreeRunStatus FAshenOathBossApproachTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.Boss = ResolveBoss(Context);
	InstanceData.TargetActor = InstanceData.Boss
		? UGameplayStatics::GetPlayerPawn(InstanceData.Boss, 0)
		: nullptr;
	InstanceData.AIController = InstanceData.Boss
		? Cast<AAIController>(InstanceData.Boss->GetController())
		: nullptr;
	InstanceData.bOwnsMoveRequest = false;

	if (!IsValid(InstanceData.Boss) ||
		!IsValid(InstanceData.TargetActor) ||
		!IsValid(InstanceData.AIController))
	{
		return EStateTreeRunStatus::Failed;
	}

	if (IsWithinRange(*InstanceData.Boss, *InstanceData.TargetActor, InstanceData.AttackRange))
	{
		InstanceData.AIController->StopMovement();
		FaceTarget(*InstanceData.Boss, *InstanceData.TargetActor);
		return EStateTreeRunStatus::Succeeded;
	}

	FAIMoveRequest MoveRequest(InstanceData.TargetActor);
	MoveRequest.SetAcceptanceRadius(InstanceData.AttackRange);
	MoveRequest.SetUsePathfinding(true);
	MoveRequest.SetAllowPartialPath(true);
	MoveRequest.SetCanStrafe(false);
	MoveRequest.SetNavigationFilter(
		InstanceData.AIController->GetDefaultNavigationFilterClass()
	);

	// AttackRange is measured between actor centers, so the navigation reach
	// test must use the same distance definition as IsWithinRange().
	MoveRequest.SetReachTestIncludesAgentRadius(false);
	MoveRequest.SetReachTestIncludesGoalRadius(false);

	const FPathFollowingRequestResult MoveResult =
		InstanceData.AIController->MoveTo(MoveRequest);

	InstanceData.bOwnsMoveRequest =
		MoveResult.Code == EPathFollowingRequestResult::RequestSuccessful;

	if (MoveResult.Code == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		FaceTarget(*InstanceData.Boss, *InstanceData.TargetActor);
		return EStateTreeRunStatus::Succeeded;
	}

	return InstanceData.bOwnsMoveRequest
		? EStateTreeRunStatus::Running
		: EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FAshenOathBossApproachTask::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!IsValid(InstanceData.Boss) ||
		!IsValid(InstanceData.TargetActor) ||
		!IsValid(InstanceData.AIController))
	{
		return EStateTreeRunStatus::Failed;
	}

	if (IsWithinRange(*InstanceData.Boss, *InstanceData.TargetActor, InstanceData.AttackRange))
	{
		FaceTarget(*InstanceData.Boss, *InstanceData.TargetActor);
		return EStateTreeRunStatus::Succeeded;
	}

	const UPathFollowingComponent* PathFollowing = InstanceData.AIController->GetPathFollowingComponent();
	return !PathFollowing || PathFollowing->GetStatus() == EPathFollowingStatus::Idle
		? EStateTreeRunStatus::Failed
		: EStateTreeRunStatus::Running;
}

void FAshenOathBossApproachTask::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (InstanceData.bOwnsMoveRequest && IsValid(InstanceData.AIController))
	{
		InstanceData.AIController->StopMovement();
	}

	InstanceData.bOwnsMoveRequest = false;
	InstanceData.AIController = nullptr;
	InstanceData.TargetActor = nullptr;
	InstanceData.Boss = nullptr;
}

FAshenOathBossSingleSwingTask::FAshenOathBossSingleSwingTask()
{
	bShouldCallTick = false;
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

EStateTreeRunStatus FAshenOathBossSingleSwingTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.Boss = ResolveBoss(Context);

	if (!IsValid(InstanceData.Boss))
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.AbilityEndedDelegateHandle = InstanceData.Boss->OnSingleSwingEnded().AddLambda(
		[WeakContext = Context.MakeWeakExecutionContext()](const bool bWasCancelled)
		{
			WeakContext.FinishTask(
				bWasCancelled
					? EStateTreeFinishTaskType::Failed
					: EStateTreeFinishTaskType::Succeeded
			);
		}
	);

	if (!InstanceData.Boss->RequestSingleSwing() || !InstanceData.Boss->IsSingleSwingActive())
	{
		InstanceData.Boss->OnSingleSwingEnded().Remove(InstanceData.AbilityEndedDelegateHandle);
		InstanceData.AbilityEndedDelegateHandle.Reset();
		InstanceData.Boss = nullptr;
		return EStateTreeRunStatus::Failed;
	}

	return EStateTreeRunStatus::Running;
}

void FAshenOathBossSingleSwingTask::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (IsValid(InstanceData.Boss))
	{
		InstanceData.Boss->OnSingleSwingEnded().Remove(InstanceData.AbilityEndedDelegateHandle);
		InstanceData.AbilityEndedDelegateHandle.Reset();
		InstanceData.Boss->CancelSingleSwing();
	}

	InstanceData.Boss = nullptr;
}

EStateTreeRunStatus FAshenOathBossRecoveryTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.ElapsedTime = 0.0f;

	return InstanceData.Duration <= 0.0f
		? EStateTreeRunStatus::Succeeded
		: EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FAshenOathBossRecoveryTask::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.ElapsedTime += FMath::Max(0.0f, DeltaTime);

	return InstanceData.ElapsedTime >= InstanceData.Duration
		? EStateTreeRunStatus::Succeeded
		: EStateTreeRunStatus::Running;
}
