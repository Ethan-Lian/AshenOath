#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "AshenOathCharacterAnimInstance.generated.h"


class UCharacterMovementComponent;

UCLASS()
class ASHENOATH_API UAshenOathCharacterAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
	
public:
	virtual void NativeInitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;
    virtual void NativeUninitializeAnimation() override;
protected:
	// Horizontal world speed in centimetres per second. Feed this to a 1D Blend Space.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "AshenOath|Animation|Locomotion")
	float GroundSpeed = 0.0f;

	// Velocity direction relative to the character: forward 0, right 90, left -90.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "AshenOath|Animation|Locomotion")
	float MovementDirection = 0.0f;

	// A small threshold prevents idle/move transitions from flickering near zero speed.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "AshenOath|Animation|Locomotion")
	bool bIsMoving = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "AshenOath|Animation|Locomotion")
	bool bIsInAir = false;

private:
	void RefreshOwnerReferences();
	void ResetLocomotionData();

	// Weak caches do not keep a previous pawn or movement component alive when an
	// AnimInstance is reinitialized, previewed, or its skeletal mesh changes owner.
	TWeakObjectPtr<ACharacter> CachedCharacter;
	TWeakObjectPtr<UCharacterMovementComponent> CachedMovementComponent;
};
