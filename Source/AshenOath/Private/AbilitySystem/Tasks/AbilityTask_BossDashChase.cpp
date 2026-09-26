#include "AbilitySystem/Tasks/AbilityTask_BossDashChase.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
	constexpr float StallCheckIntervalSeconds = 0.75f;
	constexpr float MinProgressDistanceCm = 5.0f;
}

UAbilityTask_BossDashChase::UAbilityTask_BossDashChase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTickingTask = true;
}

UAbilityTask_BossDashChase* UAbilityTask_BossDashChase::ChaseTarget(
	UGameplayAbility* OwningAbility,
	const FName TaskInstanceName,
	AActor* TargetActor,
	const float InStopDistance,
	const float InSpeedMultiplier)
{
	if (!OwningAbility)
	{
		return nullptr;
	}

	UAbilityTask_BossDashChase* Task =
		NewAbilityTask<UAbilityTask_BossDashChase>(OwningAbility, TaskInstanceName);
	Task->Target = TargetActor;
	Task->StopDistance = InStopDistance;
	Task->SpeedMultiplier = InSpeedMultiplier;
	return Task;
}

void UAbilityTask_BossDashChase::Activate()
{
	ACharacter* AvatarCharacter = Cast<ACharacter>(GetAvatarActor());
	UCharacterMovementComponent* Movement = AvatarCharacter
		? AvatarCharacter->GetCharacterMovement()
		: nullptr;
	AActor* TargetActor = Target.Get();
	if (!IsValid(AvatarCharacter) || !IsValid(Movement) || !IsValid(TargetActor) ||
		TargetActor == AvatarCharacter || AvatarCharacter->IsActorBeingDestroyed() ||
		TargetActor->IsActorBeingDestroyed() ||
		!FMath::IsFinite(StopDistance) || StopDistance <= 0.0f ||
		!FMath::IsFinite(SpeedMultiplier) || SpeedMultiplier <= 1.0f ||
		Movement->MaxWalkSpeed <= 0.0f || !Movement->IsMovingOnGround())
	{
		FinishChase(false);
		return;
	}

	Character = AvatarCharacter;
	MovementComponent = Movement;
	ProgressCheckLocation = AvatarCharacter->GetActorLocation();
	SavedMaxWalkSpeed = Movement->MaxWalkSpeed;
	bOwnsSpeedOverride = true;
	Movement->MaxWalkSpeed = SavedMaxWalkSpeed * SpeedMultiplier;
	SetWaitingOnAvatar();
}

void UAbilityTask_BossDashChase::TickTask(const float DeltaTime)
{
	Super::TickTask(DeltaTime);
	if (bChaseFinished)
	{
		return;
	}

	ACharacter* AvatarCharacter = Character.Get();
	const AActor* TargetActor = Target.Get();
	UCharacterMovementComponent* Movement = MovementComponent.Get();
	if (!IsValid(AvatarCharacter) || !IsValid(TargetActor) || !IsValid(Movement) ||
		AvatarCharacter->IsActorBeingDestroyed() || TargetActor->IsActorBeingDestroyed() ||
		!Movement->IsMovingOnGround())
	{
		FinishChase(false);
		return;
	}

	const FVector ToTarget = TargetActor->GetActorLocation() - AvatarCharacter->GetActorLocation();
	if (ToTarget.SizeSquared2D() <= FMath::Square(StopDistance))
	{
		FinishChase(true);
		return;
	}

	const FVector Direction = ToTarget.GetSafeNormal2D();
	AvatarCharacter->SetActorRotation(FRotator(0.0f, Direction.Rotation().Yaw, 0.0f));
	AvatarCharacter->AddMovementInput(Direction);

	ProgressCheckElapsed += FMath::Clamp(DeltaTime, 0.0f, 0.1f);
	if (ProgressCheckElapsed >= StallCheckIntervalSeconds)
	{
		if (FVector::DistSquared2D(ProgressCheckLocation, AvatarCharacter->GetActorLocation()) <
			FMath::Square(MinProgressDistanceCm))
		{
			FinishChase(false);
			return;
		}

		ProgressCheckLocation = AvatarCharacter->GetActorLocation();
		ProgressCheckElapsed = 0.0f;
	}
}

void UAbilityTask_BossDashChase::OnDestroy(const bool bInOwnerFinished)
{
	ReleaseMovement();
	Target.Reset();
	Character.Reset();
	MovementComponent.Reset();
	Super::OnDestroy(bInOwnerFinished);
}

void UAbilityTask_BossDashChase::FinishChase(const bool bReached)
{
	if (bChaseFinished)
	{
		return;
	}

	bChaseFinished = true;
	ReleaseMovement();
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		if (bReached)
		{
			OnReached.Broadcast();
		}
		else
		{
			OnFailed.Broadcast();
		}
	}

	if (!IsFinished())
	{
		EndTask();
	}
}

void UAbilityTask_BossDashChase::ReleaseMovement()
{
	if (!bOwnsSpeedOverride)
	{
		return;
	}

	bOwnsSpeedOverride = false;
	if (UCharacterMovementComponent* Movement = MovementComponent.Get())
	{
		Movement->StopMovementImmediately();
		Movement->MaxWalkSpeed = SavedMaxWalkSpeed;
	}
	if (ACharacter* AvatarCharacter = Character.Get())
	{
		AvatarCharacter->ConsumeMovementInputVector();
	}
}
