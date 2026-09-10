#include "Game/AshenOathGameMode.h"

#include "Characters/AshenOathBossCharacter.h"
#include "Characters/AshenOathPlayerCharacter.h"
#include "Player/AshenOathPlayerController.h"
#include "UI/AshenOathHUD.h"

AAshenOathGameMode::AAshenOathGameMode()
{
	DefaultPawnClass = AAshenOathPlayerCharacter::StaticClass();
	PlayerControllerClass = AAshenOathPlayerController::StaticClass();
	HUDClass = AAshenOathHUD::StaticClass();
}

bool AAshenOathGameMode::RegisterBoss(AAshenOathBossCharacter* Boss)
{
	if (!IsValid(Boss))
	{
		return false;
	}

	AAshenOathBossCharacter* CurrentBoss = ActiveBoss.Get();


	// Avoid duplicate Boss registration and repeated notifications to listeners.
    if (CurrentBoss == Boss)
    {
        return true;
    }
	
	// Only one active Boss is supported; report conflicting registrations in development.
	if (!ensureMsgf(!IsValid(CurrentBoss), TEXT("AshenOath currently supports only one active boss.")))
	{
		return false;
	}

	ActiveBoss = Boss;
	ActiveBossChangedEvent.Broadcast(Boss);

	return true;
}

void AAshenOathGameMode::UnregisterBoss(AAshenOathBossCharacter* Boss)
{
	if (ActiveBoss.Get() != Boss)
	{
		return;
	}

	ActiveBoss.Reset();
	ActiveBossChangedEvent.Broadcast(nullptr);
}

AAshenOathBossCharacter* AAshenOathGameMode::GetActiveBoss() const
{
	return ActiveBoss.Get();
}
