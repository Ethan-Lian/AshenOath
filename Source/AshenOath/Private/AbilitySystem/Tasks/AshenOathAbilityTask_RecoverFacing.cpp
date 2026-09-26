#include "AbilitySystem/Tasks/AshenOathAbilityTask_RecoverFacing.h"

UAshenOathAbilityTask_RecoverFacing::UAshenOathAbilityTask_RecoverFacing(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTickingTask = true;
}

UAshenOathAbilityTask_RecoverFacing*
	UAshenOathAbilityTask_RecoverFacing::RecoverFacing(
		UGameplayAbility* OwningAbility,
		const FName TaskInstanceName,
		AActor* FacingTargetActor,
		const float StartOffsetSeconds,
		const float DurationSeconds,
		const double ExecutionStartWorldTime)
{
	if (!OwningAbility || !FacingTargetActor)
	{
		return nullptr;
	}

	UAshenOathAbilityTask_RecoverFacing* Task =
		NewAbilityTask<UAshenOathAbilityTask_RecoverFacing>(
			OwningAbility,
			TaskInstanceName
		);
	Task->FacingTarget = FacingTargetActor;
	Task->RecoveryStartOffset = StartOffsetSeconds;
	Task->RecoveryDuration = DurationSeconds;
	Task->ExecutionStartTime = ExecutionStartWorldTime;
	return Task;
}

void UAshenOathAbilityTask_RecoverFacing::Activate()
{
	AActor* Avatar = GetAvatarActor();
	AActor* Target = FacingTarget.Get();
	if (!IsValid(Avatar) ||
		Avatar->IsActorBeingDestroyed() ||
		!IsValid(Target) ||
		Target->IsActorBeingDestroyed() ||
		RecoveryStartOffset < 0.0f ||
		RecoveryDuration <= KINDA_SMALL_NUMBER ||
		!GetWorld())
	{
		EndTask();
		return;
	}

	AvatarActor = Avatar;
	SetWaitingOnAvatar();

	// A zero-offset recovery should take ownership in the activation frame.
	TickTask(0.0f);
}

void UAshenOathAbilityTask_RecoverFacing::TickTask(const float DeltaTime)
{
	Super::TickTask(DeltaTime);

	if (IsFinished())
	{
		return;
	}

	UWorld* World = GetWorld();
	AActor* Avatar = AvatarActor.Get();
	AActor* Target = FacingTarget.Get();
	if (!World ||
		!IsValid(Avatar) ||
		Avatar->IsActorBeingDestroyed() ||
		!IsValid(Target) ||
		Target->IsActorBeingDestroyed())
	{
		EndTask();
		return;
	}

	const double ElapsedTime = World->GetTimeSeconds() - ExecutionStartTime;
	if (ElapsedTime < RecoveryStartOffset)
	{
		return;
	}

	if (!bRecoveryStarted)
	{
		RecoveryStartYaw = Avatar->GetActorRotation().Yaw;
		bRecoveryStarted = true;
	}

	const FVector DirectionToTarget = (
		Target->GetActorLocation() - Avatar->GetActorLocation()
	).GetSafeNormal2D();
	if (DirectionToTarget.SizeSquared2D() <= SMALL_NUMBER)
	{
		return;
	}

	const float RecoveryAlpha = FMath::Clamp(
		static_cast<float>((ElapsedTime - RecoveryStartOffset) / RecoveryDuration),
		0.0f,
		1.0f
	);
	const float EasedAlpha = FMath::SmoothStep(0.0f, 1.0f, RecoveryAlpha);
	const float DesiredYaw = DirectionToTarget.Rotation().Yaw;
	const float RecoveredYaw = RecoveryStartYaw +
		FMath::FindDeltaAngleDegrees(RecoveryStartYaw, DesiredYaw) * EasedAlpha;

	Avatar->SetActorRotation(FRotator(0.0f, RecoveredYaw, 0.0f));

	if (RecoveryAlpha >= 1.0f)
	{
		EndTask();
	}
}

void UAshenOathAbilityTask_RecoverFacing::OnDestroy(const bool bInOwnerFinished)
{
	AvatarActor.Reset();
	FacingTarget.Reset();
	Super::OnDestroy(bInOwnerFinished);
}
