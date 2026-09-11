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
 * Owns local player input and routes player intent to the currently possessed Character.
 *
 * The Controller is responsible for Enhanced Input bindings and MappingContext lifetime,
 * while gameplay rules remain on the Character and its gameplay components.
 *
 * Input flow:
 *
 * LocalPlayer
 *     ↓
 * PlayerController
 *     ├── MappingContext registration
 *     ├── InputAction bindings
 *     └── HandleXxx()
 *             ↓
 *     Possessed Character
 *             ↓
 *     RequestXxx()
 *             ↓
 * CharacterMovement / Camera / CombatActionComponent
 */
UCLASS()
class ASHENOATH_API AAshenOathPlayerController : public APlayerController
{
	GENERATED_BODY()
	
protected:
    // Bind Enhanced Input callbacks once the controller's InputComponent is available.
    virtual void SetupInputComponent() override;

    // Refresh gameplay input after the controller gains a pawn.
    virtual void OnPossess(APawn* InPawn) override;

    // Release controller-owned gameplay input before possession is lost.
    virtual void OnUnPossess() override;

    // Final cleanup path if the controller leaves play while mappings are still registered.
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

	// Input bindings belong to a specific InputComponent.
    // Possession may change independently, so avoid binding the same component twice.
	TWeakObjectPtr<UEnhancedInputComponent> BoundInputComponent;

	// Registration records only: weak references do not extend object lifetimes.
	// Keep the actual PlayerInput identity because the LocalPlayer subsystem may
	// later serve a different controller. Get() returns null for an invalid target.
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> RegisteredInputSubsystem;
	TWeakObjectPtr<UEnhancedPlayerInput> RegisteredPlayerInput;
	TWeakObjectPtr<UInputMappingContext> RegisteredMappingContext;

	// Enhanced Input callbacks. They translate raw input into Character-level requests.
	void HandleMove(const FInputActionValue& Value);
	void HandleLook(const FInputActionValue& Value);
	void HandleLightAttack();

	// Input setup and possession become ready independently.
    // These helpers keep MappingContext registration idempotent across both lifecycle paths.
	void RefreshGameplayInputMapping();
	void RemoveGameplayInputMapping();
	
};
