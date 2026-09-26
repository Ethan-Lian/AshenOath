#include "Player/AshenOathPlayerController.h"

#include "Characters/AshenOathPlayerCharacter.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Game/AshenOathGameMode.h"

bool AAshenOathPlayerController::RequestRetry()
{
	UWorld* World = GetWorld();
	AAshenOathGameMode* GameMode = World
		? World->GetAuthGameMode<AAshenOathGameMode>()
		: nullptr;

	return IsLocalController() &&
		GameMode &&
		GameMode->RequestRetry(this);
}

void AAshenOathPlayerController::SetGameplayInputEnabled(const bool bEnabled)
{
	if (bGameplayInputEnabled == bEnabled)
	{
		return;
	}

	bGameplayInputEnabled = bEnabled;
	CurrentMovementIntent = FVector2D::ZeroVector;
	SetIgnoreMoveInput(!bEnabled);
	SetIgnoreLookInput(!bEnabled);

	if (!bEnabled)
	{
		if (AAshenOathPlayerCharacter* PlayerCharacter =
			Cast<AAshenOathPlayerCharacter>(GetPawn()))
		{
			PlayerCharacter->RequestClearLockOn();
		}
	}

	if (bEnabled)
	{
		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(false);
		RefreshGameplayInputMapping();
	}
	else
	{
		RemoveGameplayInputMapping();
		SetInputMode(FInputModeGameAndUI());
		SetShowMouseCursor(true);
	}
}

void AAshenOathPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);

	if (!EnhancedInput || !GameplayMappingContext)
	{
		return;
	}

	if (BoundInputComponent.Get() != EnhancedInput)
	{
		// Bind each configured action independently. A missing new action should
		// not disable movement or previously configured combat input.
		if (MoveAction && MoveAction->ValueType == EInputActionValueType::Axis2D)
		{
			EnhancedInput->BindAction(
				MoveAction, ETriggerEvent::Triggered,
				this, &AAshenOathPlayerController::HandleMove
			);
			EnhancedInput->BindAction(
				MoveAction, ETriggerEvent::Completed,
				this, &AAshenOathPlayerController::HandleMove
			);
			EnhancedInput->BindAction(
				MoveAction, ETriggerEvent::Canceled,
				this, &AAshenOathPlayerController::HandleMove
			);
		}

		if (LookAction && LookAction->ValueType == EInputActionValueType::Axis2D)
		{
			EnhancedInput->BindAction(
				LookAction, ETriggerEvent::Triggered,
				this, &AAshenOathPlayerController::HandleLook
			);
		}

		if (ComboAttackAction && ComboAttackAction->ValueType == EInputActionValueType::Boolean)
		{
			EnhancedInput->BindAction(
				ComboAttackAction, ETriggerEvent::Started,
				this, &AAshenOathPlayerController::HandleComboAttack
			);
		}

		if (DodgeAction && DodgeAction->ValueType == EInputActionValueType::Boolean)
		{
			EnhancedInput->BindAction(
				DodgeAction, ETriggerEvent::Started,
				this, &AAshenOathPlayerController::HandleDodge
			);
		}

		if (HeavyAttackAction && HeavyAttackAction->ValueType == EInputActionValueType::Boolean)
		{
			EnhancedInput->BindAction(
				HeavyAttackAction, ETriggerEvent::Started,
				this, &AAshenOathPlayerController::HandleHeavyAttackPressed
			);
			EnhancedInput->BindAction(
				HeavyAttackAction, ETriggerEvent::Completed,
				this, &AAshenOathPlayerController::HandleHeavyAttackReleased
			);
			EnhancedInput->BindAction(
				HeavyAttackAction, ETriggerEvent::Canceled,
				this, &AAshenOathPlayerController::HandleHeavyAttackReleased
			);
		}

		if (HealAction && HealAction->ValueType == EInputActionValueType::Boolean)
		{
			EnhancedInput->BindAction(
				HealAction, ETriggerEvent::Started,
				this, &AAshenOathPlayerController::HandleHeal
			);
		}

		if (LockOnAction && LockOnAction->ValueType == EInputActionValueType::Boolean)
		{
			EnhancedInput->BindAction(
				LockOnAction, ETriggerEvent::Started,
				this, &AAshenOathPlayerController::HandleToggleLockOn
			);
		}

		BoundInputComponent = EnhancedInput;
	}

	RefreshGameplayInputMapping();
}

// Super establishes the control relationship and calls Character::PossessedBy,
// where GAS ActorInfo is refreshed. Check mappings after that relationship is ready.
void AAshenOathPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	RefreshGameplayInputMapping();

	if (bGameplayInputEnabled && IsLocalController() && GetLocalPlayer() &&
		Cast<AAshenOathPlayerCharacter>(GetPawn()))
	{
		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(false);
	}
}

void AAshenOathPlayerController::OnUnPossess()
{
	CurrentMovementIntent = FVector2D::ZeroVector;
	RemoveGameplayInputMapping();
	Super::OnUnPossess();
}

void AAshenOathPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveGameplayInputMapping();
	Super::EndPlay(EndPlayReason);
}

void AAshenOathPlayerController::HandleMove(const FInputActionValue& Value)
{
	if (!IsLocalController())
	{
		return;
	}

	CurrentMovementIntent = bGameplayInputEnabled
		? Value.Get<FVector2D>()
		: FVector2D::ZeroVector;

	if (!bGameplayInputEnabled || IsMoveInputIgnored())
	{
		return;
	}

	AAshenOathPlayerCharacter* PlayerCharacter = Cast<AAshenOathPlayerCharacter>(GetPawn());

	if (IsValid(PlayerCharacter))
	{
		PlayerCharacter->RequestMove(
			CurrentMovementIntent,
			GetControlRotation().Yaw
		);
	}
}

void AAshenOathPlayerController::HandleLook(const FInputActionValue& Value)
{
	AAshenOathPlayerCharacter* PlayerCharacter =
		Cast<AAshenOathPlayerCharacter>(GetPawn());

	if (!bGameplayInputEnabled || !IsLocalController() || IsLookInputIgnored() ||
		!IsValid(PlayerCharacter) || PlayerCharacter->IsLockedOn())
	{
		return;
	}

	const FVector2D LookInput = Value.Get<FVector2D>();
	AddYawInput(LookInput.X);
	AddPitchInput(LookInput.Y);
}

void AAshenOathPlayerController::RefreshGameplayInputMapping()
{
	// Input setup and possession can become ready separately. Both call this helper,
	// which must leave one valid registration even when called more than once.
	if (!bGameplayInputEnabled || !IsLocalController() || !GetLocalPlayer() ||
		!BoundInputComponent.IsValid() ||
		BoundInputComponent.Get() != InputComponent.Get() ||
		!IsValid(Cast<AAshenOathPlayerCharacter>(GetPawn())))
	{
		RemoveGameplayInputMapping();
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(
			GetLocalPlayer()
		);

	UEnhancedPlayerInput* CurrentPlayerInput =
		Subsystem ? Subsystem->GetPlayerInput() : nullptr;

	if (!CurrentPlayerInput || !GameplayMappingContext)
	{
		return;
	}

	// Match our registration record as well as current presence. A context that
	// exists without that record may belong to another system and must not be claimed.
	if (RegisteredInputSubsystem.Get() == Subsystem &&
		RegisteredPlayerInput.Get() == CurrentPlayerInput &&
		RegisteredMappingContext.Get() == GameplayMappingContext.Get() &&
		Subsystem->HasMappingContext(GameplayMappingContext.Get()))
	{
		return;
	}

	RemoveGameplayInputMapping();

	if (Subsystem->HasMappingContext(GameplayMappingContext.Get()))
	{
		return;
	}

	Subsystem->AddMappingContext(GameplayMappingContext.Get(), 0);
	RegisteredInputSubsystem = Subsystem;
	RegisteredPlayerInput = CurrentPlayerInput;
	RegisteredMappingContext = GameplayMappingContext.Get();
}

void AAshenOathPlayerController::RemoveGameplayInputMapping()
{
	UEnhancedInputLocalPlayerSubsystem* Subsystem = RegisteredInputSubsystem.Get();
	UEnhancedPlayerInput* InstalledPlayerInput = RegisteredPlayerInput.Get();
	UInputMappingContext* MappingContext = RegisteredMappingContext.Get();

	// A LocalPlayer can outlive this controller and point at a different PlayerInput.
	// Removing through that new input object would alter the next controller's mappings.
	if (Subsystem && InstalledPlayerInput && MappingContext &&
		Subsystem->GetPlayerInput() == InstalledPlayerInput)
	{
		Subsystem->RemoveMappingContext(MappingContext);
	}

	// Reset clears our references only. A later UnPossess/EndPlay cleanup becomes a no-op;
	// the referenced objects still have their lifetimes managed by the engine.
	RegisteredInputSubsystem.Reset();
	RegisteredPlayerInput.Reset();
	RegisteredMappingContext.Reset();
}

void AAshenOathPlayerController::HandleComboAttack()
{
	if (!bGameplayInputEnabled || !IsLocalController())
	{
		return;
	}

	AAshenOathPlayerCharacter* PlayerCharacter = Cast<AAshenOathPlayerCharacter>(GetPawn());

	if (IsValid(PlayerCharacter))
	{
		PlayerCharacter->RequestComboAttack();
	}
}

void AAshenOathPlayerController::HandleDodge()
{
	if (!bGameplayInputEnabled || !IsLocalController())
	{
		return;
	}

	AAshenOathPlayerCharacter* PlayerCharacter = Cast<AAshenOathPlayerCharacter>(GetPawn());

	if (IsValid(PlayerCharacter))
	{
		PlayerCharacter->RequestDodge(CurrentMovementIntent);
	}
}

void AAshenOathPlayerController::HandleHeavyAttackPressed()
{
	if (!bGameplayInputEnabled || !IsLocalController())
	{
		return;
	}

	if (AAshenOathPlayerCharacter* PlayerCharacter =
		Cast<AAshenOathPlayerCharacter>(GetPawn()))
	{
		PlayerCharacter->RequestHeavyAttackPressed();
	}
}

void AAshenOathPlayerController::HandleHeavyAttackReleased()
{
	if (!bGameplayInputEnabled || !IsLocalController())
	{
		return;
	}

	if (AAshenOathPlayerCharacter* PlayerCharacter =
		Cast<AAshenOathPlayerCharacter>(GetPawn()))
	{
		PlayerCharacter->RequestHeavyAttackReleased();
	}
}

void AAshenOathPlayerController::HandleHeal()
{
	if (!bGameplayInputEnabled || !IsLocalController())
	{
		return;
	}

	if (AAshenOathPlayerCharacter* PlayerCharacter =
		Cast<AAshenOathPlayerCharacter>(GetPawn()))
	{
		PlayerCharacter->RequestHeal();
	}
}

void AAshenOathPlayerController::HandleToggleLockOn()
{
	if (!bGameplayInputEnabled || !IsLocalController())
	{
		return;
	}

	if (AAshenOathPlayerCharacter* PlayerCharacter =
		Cast<AAshenOathPlayerCharacter>(GetPawn()))
	{
		PlayerCharacter->RequestToggleLockOn();
	}
}

