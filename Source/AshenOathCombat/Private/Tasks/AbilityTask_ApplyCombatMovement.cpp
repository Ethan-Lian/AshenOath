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
		FHitResult Hit;
		Movement->SafeMoveUpdatedComponent(
			Delta,
			AvatarCharacter->GetActorQuat(),
			true,
			Hit
		);

		if (IsFinished())
		{
			return;
		}

		// SafeMoveUpdatedComponent applies only the collision-valid portion. Keep
		// advancing the timeline so floor contact is not mistaken for a wall; an
		// actual wall naturally truncates the accumulated displacement.
	}

	PreviousMovementAlpha = NewMovementAlpha;

	if (NewMovementAlpha >= 1.0f)
	{
		bMovementFinished = true;
		EndMovementControl();
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
