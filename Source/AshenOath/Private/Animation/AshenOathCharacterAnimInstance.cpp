#include "Animation/AshenOathCharacterAnimInstance.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace AshenOathLocomotion
{
	constexpr float MovingSpeedThreshold = 3.0f;
}

void UAshenOathCharacterAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	
	ResetLocomotionData();
	RefreshOwnerReferences();
}

void UAshenOathCharacterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	
	// A runtime mesh can be reinitialized with a different owner.
	// Recheck identity instead of trusting the initial cache forever.
	if (TryGetPawnOwner() != CachedCharacter.Get() ||
		!CachedMovementComponent.IsValid())
	{
		RefreshOwnerReferences();
	}

	const ACharacter* Character = CachedCharacter.Get();
	const UCharacterMovementComponent* MovementComponent = CachedMovementComponent.Get();

	if (!Character || !MovementComponent)
	{
		ResetLocomotionData();
		return;
	}

	const FVector HorizontalVelocity(
		MovementComponent->Velocity.X,
		MovementComponent->Velocity.Y,
		0.0f
	);

	GroundSpeed = HorizontalVelocity.Size();
	bIsMoving = GroundSpeed > AshenOathLocomotion::MovingSpeedThreshold;
	bIsInAir = MovementComponent->IsFalling();

	if (!bIsMoving)
	{
		MovementDirection = 0.0f;
		return;
	}

	// Transform world velocity into the character's local frame. atan2(Y, X)
	// then produces the conventional Blend Space direction range [-180, 180].
	const FVector LocalVelocity =
		Character->GetActorTransform().InverseTransformVectorNoScale(
			HorizontalVelocity
		);

	MovementDirection = FMath::RadiansToDegrees(
		FMath::Atan2(LocalVelocity.Y, LocalVelocity.X)
	);
}

void UAshenOathCharacterAnimInstance::NativeUninitializeAnimation()
{
	CachedMovementComponent.Reset();
	CachedCharacter.Reset();
	ResetLocomotionData();

	Super::NativeUninitializeAnimation();
}

void UAshenOathCharacterAnimInstance::RefreshOwnerReferences()
{
	ACharacter* Character = Cast<ACharacter>(TryGetPawnOwner());

	CachedCharacter = Character;
	
	CachedMovementComponent = Character? Character->GetCharacterMovement() : nullptr;
}

void UAshenOathCharacterAnimInstance::ResetLocomotionData()
{
	GroundSpeed = 0.0f;
	MovementDirection = 0.0f;
	bIsInAir = false;
	bIsMoving = false;
}
