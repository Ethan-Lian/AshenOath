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
	// Horizontal speed in cm/s.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "AshenOath|Animation|Locomotion")
	float GroundSpeed = 0.0f;

	// Local movement angle in degrees: forward 0, right 90, left -90.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "AshenOath|Animation|Locomotion")
	float MovementDirection = 0.0f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "AshenOath|Animation|Locomotion")
	bool bIsMoving = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "AshenOath|Animation|Locomotion")
	bool bIsInAir = false;

private:
	void RefreshOwnerReferences();
	void ResetLocomotionData();

	TWeakObjectPtr<ACharacter> CachedCharacter;

	TWeakObjectPtr<UCharacterMovementComponent> CachedMovementComponent;
};
