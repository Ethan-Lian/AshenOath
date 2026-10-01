#include "Tasks/AbilityTask_ApplyCombatMovement.h"

#include "Animation/AnimInstance.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UAbilityTask_ApplyCombatMovement::UAbilityTask_ApplyCombatMovement(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTickingTask = true;
}

UAbilityTask_ApplyCombatMovement* UAbilityTask_ApplyCombatMovement::ApplyCombatMovement(
	UGameplayAbility* OwningAbility,
	const FName TaskInstanceName,
	const FVector WorldDirection,
	const float Distance,
	const float StartOffsetSeconds,
	const float DurationSeconds,
	const double ExecutionStartWorldTime)
{
	if (!OwningAbility)
	{
		return nullptr;
	}

	UAbilityTask_ApplyCombatMovement* Task =
		NewAbilityTask<UAbilityTask_ApplyCombatMovement>(OwningAbility, TaskInstanceName);

	Task->MovementDirection = WorldDirection.GetSafeNormal2D();
	Task->MovementDistance = Distance;
	Task->MovementStartOffset = StartOffsetSeconds;
	Task->MovementDuration = DurationSeconds;
	Task->ExecutionStartTime = ExecutionStartWorldTime;
	return Task;
}

void UAbilityTask_ApplyCombatMovement::Activate()
{
	ACharacter* AvatarCharacter = Cast<ACharacter>(GetAvatarActor());
	UCharacterMovementComponent* Movement =
		AvatarCharacter ? AvatarCharacter->GetCharacterMovement() : nullptr;
	UAnimInstance* AvatarAnimInstance = AvatarCharacter && AvatarCharacter->GetMesh()
		? AvatarCharacter->GetMesh()->GetAnimInstance()
		: nullptr;

	if (!IsValid(AvatarCharacter) ||
		AvatarCharacter->IsActorBeingDestroyed() ||
		!IsValid(Movement) ||
		!IsValid(AvatarAnimInstance) ||
		MovementDirection.SizeSquared2D() <= SMALL_NUMBER ||
		MovementDistance <= KINDA_SMALL_NUMBER ||
		MovementStartOffset < 0.0f ||
		MovementDuration <= KINDA_SMALL_NUMBER ||
		!GetWorld())
	{
		FailTask();
		return;
	}

	Character = AvatarCharacter;
	MovementComponent = Movement;
	AnimInstance = AvatarAnimInstance;

	SavedRootMotionMode = static_cast<uint8>(AvatarAnimInstance->RootMotionMode.GetValue());
	bRootMotionModeOverridden = true;
	AvatarAnimInstance->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);

	if (IsFinished())
	{
		return;
	}

	SetWaitingOnAvatar();

	// Handle zero-offset movement without waiting for the following frame.
	TickTask(0.0f);
}

void UAbilityTask_ApplyCombatMovement::TickTask(const float DeltaTime)
{
	Super::TickTask(DeltaTime);

	if (bMovementFinished)
	{
		return;
	}

	UWorld* World = GetWorld();
	ACharacter* AvatarCharacter = Character.Get();
	UCharacterMovementComponent* Movement = MovementComponent.Get();

	if (!World || !AvatarCharacter || !Movement || AvatarCharacter->IsActorBeingDestroyed())
	{
		FailTask();
		return;
	}

	const double ElapsedTime = World->GetTimeSeconds() - ExecutionStartTime;
	const float NewMovementAlpha = FMath::Clamp(
		static_cast<float>((ElapsedTime - MovementStartOffset) / MovementDuration),
		0.0f,
		1.0f
	);

	if (NewMovementAlpha > PreviousMovementAlpha)
	{
		BeginMovementControl();

		if (IsFinished())
		{
			return;
		}

		if (!bMovementControlActive)
		{
			FailTask();
			return;
		}

		const FVector Delta = MovementDirection * MovementDistance *
			(NewMovementAlpha - PreviousMovementAlpha);
		ApplyMovementDelta(Delta);

		if (IsFinished())
		{
			return;
		}

		// Blocked distance is not carried into later frames or added to the end.
	}

	PreviousMovementAlpha = NewMovementAlpha;

	if (NewMovementAlpha >= 1.0f)
	{
		bMovementFinished = true;
		EndMovementControl();
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnCompleted.Broadcast();
		}
	}
}

FVector UAbilityTask_ApplyCombatMovement::ComputeFloorDelta(
	const FVector& Delta, const FHitResult& FloorHit) const
{
	const UCharacterMovementComponent* Movement = MovementComponent.Get();
	if (Movement->IsWalkable(FloorHit) && FloorHit.ImpactNormal.Z > KINDA_SMALL_NUMBER)
	{
		// Preserve the frozen XY direction and distance while following the plane.
		return FVector(Delta.X, Delta.Y,
			-FVector::DotProduct(Delta, FloorHit.ImpactNormal) / FloorHit.ImpactNormal.Z);
	}
	return Delta;
}

void UAbilityTask_ApplyCombatMovement::ApplyMovementDelta(const FVector& Delta)
{
	UCharacterMovementComponent* Movement = MovementComponent.Get();
	ACharacter* AvatarCharacter = Character.Get();
	const bool bGroundMovement = SavedMovementMode == MOVE_Walking || SavedMovementMode == MOVE_NavWalking;
	FFindFloorResult Floor;
	FVector TravelDelta = Delta;
	if (bGroundMovement)
	{
		Movement->FindFloor(AvatarCharacter->GetActorLocation(), Floor, false);
		if (Floor.IsWalkableFloor() && !Floor.bLineTrace)
		{
			TravelDelta = ComputeFloorDelta(Delta, Floor.HitResult);
		}
	}

	FHitResult Hit;
	Movement->SafeMoveUpdatedComponent(TravelDelta, AvatarCharacter->GetActorQuat(), true, Hit);
	if (IsFinished())
	{
		return;
	}

	float RemainingTime = 1.0f - Hit.Time;
	if (bGroundMovement && Floor.IsWalkableFloor() && Hit.IsValidBlockingHit())
	{
		// A flat floor can lead into a ramp within this frame's sweep.
		if (Movement->IsWalkable(Hit))
		{
			TravelDelta = ComputeFloorDelta(Delta * RemainingTime, Hit);
			Movement->SafeMoveUpdatedComponent(TravelDelta, AvatarCharacter->GetActorQuat(), true, Hit);
			if (IsFinished())
			{
				return;
			}
			RemainingTime *= 1.0f - Hit.Time;
		}
		if (Hit.IsValidBlockingHit() && Movement->CanStepUp(Hit))
		{
			// StepUp retains UE's height, clearance, walkability and collision checks.
			Movement->StepUp(Movement->GetGravityDirection(), Delta * RemainingTime, Hit);
			if (IsFinished())
			{
				return;
			}
		}
	}

	if (bGroundMovement)
	{
		Movement->FindFloor(AvatarCharacter->GetActorLocation(), Movement->CurrentFloor, false);
		Movement->AdjustFloorHeight();
	}
}

void UAbilityTask_ApplyCombatMovement::OnDestroy(const bool bInOwnerFinished)
{
	EndMovementControl();
	RestoreRootMotionMode();
	Character.Reset();
	MovementComponent.Reset();
	AnimInstance.Reset();
	Super::OnDestroy(bInOwnerFinished);
}

void UAbilityTask_ApplyCombatMovement::BeginMovementControl()
{
	if (bMovementControlActive)
	{
		return;
	}

	ACharacter* AvatarCharacter = Character.Get();
	UCharacterMovementComponent* Movement = MovementComponent.Get();

	if (!AvatarCharacter || !Movement)
	{
		return;
	}

	SavedMovementMode = static_cast<uint8>(Movement->MovementMode.GetValue());
	SavedCustomMovementMode = Movement->CustomMovementMode;
	// Mark ownership before calls that can synchronously notify gameplay code.
	// If such a callback ends the Ability, OnDestroy can now restore this state.
	bMovementControlActive = true;
	Movement->StopMovementImmediately();

	if (IsFinished())
	{
		return;
	}

	AvatarCharacter->ConsumeMovementInputVector();

	if (IsFinished())
	{
		return;
	}

	Movement->SetMovementMode(MOVE_None);
}

void UAbilityTask_ApplyCombatMovement::EndMovementControl()
{
	if (!bMovementControlActive)
	{
		return;
	}

	bMovementControlActive = false;
	ACharacter* AvatarCharacter = Character.Get();
	UCharacterMovementComponent* Movement = MovementComponent.Get();

	if (!AvatarCharacter || !Movement || AvatarCharacter->IsActorBeingDestroyed())
	{
		return;
	}

	Movement->StopMovementImmediately();
	AvatarCharacter->ConsumeMovementInputVector();
	Movement->SetMovementMode(
		static_cast<EMovementMode>(SavedMovementMode),
		SavedCustomMovementMode
	);
}

void UAbilityTask_ApplyCombatMovement::RestoreRootMotionMode()
{
	if (!bRootMotionModeOverridden)
	{
		return;
	}

	bRootMotionModeOverridden = false;

	if (UAnimInstance* AvatarAnimInstance = AnimInstance.Get())
	{
		AvatarAnimInstance->SetRootMotionMode(
			static_cast<ERootMotionMode::Type>(SavedRootMotionMode)
		);
	}
}

void UAbilityTask_ApplyCombatMovement::FailTask()
{
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnFailed.Broadcast();
	}

	if (!IsFinished())
	{
		EndTask();
	}
}
