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
 * Owns local input bindings and mapping registration, then routes intent to the
 * current Character. Movement rules and camera components stay with the body.
   
     Local Player
	      ↓
  PlayerController
	├── Register Mapping Context
	├── Bind InputAction
	├── HandleFunction
			↓
	Current Possess Pawn/Character
			↓
	RequestFunction
			↓
	CharacterMovement / Camera / CombatActionComponent
 */
UCLASS()
class ASHENOATH_API AAshenOathPlayerController : public APlayerController
{
	GENERATED_BODY()
	
protected:
	// Engine callbacks: input setup and possession enable gameplay input when ready;
	// unpossession and EndPlay release the mapping owned by this controller.
	
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

	// Bind once per inputcomponent; possession can change independently.
	TWeakObjectPtr<UEnhancedInputComponent> BoundInputComponent;

	// Registration records only: weak references do not extend object lifetimes.
	// Keep the actual PlayerInput identity because the LocalPlayer subsystem may
	// later serve a different controller. Get() returns null for an invalid target.
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> RegisteredInputSubsystem;
	TWeakObjectPtr<UEnhancedPlayerInput> RegisteredPlayerInput;
	TWeakObjectPtr<UInputMappingContext> RegisteredMappingContext;

	// Invoked by Enhanced Input through the bindings made in SetupInputComponent.
	void HandleMove(const FInputActionValue& Value);
	void HandleLook(const FInputActionValue& Value);
	void HandleLightAttack();

	// Shared helpers must tolerate repeated calls from different lifecycle paths.
	void RefreshGameplayInputMapping();
	void RemoveGameplayInputMapping();
	
};
