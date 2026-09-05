#include "Game/AshenOathGameMode.h"

#include "Characters/AshenOathPlayerCharacter.h"
#include "Player/AshenOathPlayerController.h"

AAshenOathGameMode::AAshenOathGameMode()
{
	DefaultPawnClass = AAshenOathPlayerCharacter::StaticClass();
	PlayerControllerClass = AAshenOathPlayerController::StaticClass();
}
