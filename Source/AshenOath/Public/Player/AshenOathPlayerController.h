#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "AshenOathPlayerController.generated.h"

class UEnhancedInputComponent;
class UEnhancedInputLocalPlayerSubsystem;
class UEnhancedPlayerInput;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;


/**
 * Owns local Enhanced Input state and routes player intent to the possessed Character.
 * Gameplay rules remain on the Character and its gameplay modules.
 */
UCLASS()
class ASHENOATH_API AAshenOathPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void SetupInputComponent() override;

	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Input")
	TObjectPtr<UInputMappingContext> GameplayMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Input")
	TObjectPtr<UInputAction> LightAttackAction;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Input")
	TObjectPtr<UInputAction> DodgeAction;

	// Input bindings belong to a specific InputComponent.
	// Possession may change independently, so avoid binding the same component twice.
	TWeakObjectPtr<UEnhancedInputComponent> BoundInputComponent;

	// Registration records only: weak references do not extend object lifetimes.
	// Keep the actual PlayerInput identity because the LocalPlayer subsystem may
	// later serve a different controller. Get() returns null for an invalid target.
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> RegisteredInputSubsystem;
	TWeakObjectPtr<UEnhancedPlayerInput> RegisteredPlayerInput;
	TWeakObjectPtr<UInputMappingContext> RegisteredMappingContext;

	// The dodge request samples the latest two-dimensional movement intent.
	// Completed/Canceled input events reset this to zero when movement is released.
	FVector2D CurrentMovementIntent = FVector2D::ZeroVector;

	void HandleMove(const FInputActionValue& Value);
	void HandleLook(const FInputActionValue& Value);
	void HandleLightAttack();
	void HandleDodge();

	// Input setup and possession become ready independently.
	// These helpers keep MappingContext registration idempotent across both lifecycle paths.
	void RefreshGameplayInputMapping();
	void RemoveGameplayInputMapping();
};
