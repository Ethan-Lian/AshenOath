#include "Game/AshenOathGameMode.h"

#include "Characters/AshenOathBossCharacter.h"
#include "Characters/AshenOathPlayerCharacter.h"
#include "Player/AshenOathPlayerController.h"
#include "UI/AshenOathHUD.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

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

EAshenOathMatchOutcome AAshenOathGameMode::GetMatchOutcome() const
{
	return MatchOutcome;
}

void AAshenOathGameMode::ReportPlayerDeath(
	AAshenOathPlayerCharacter* Player)
{
	UWorld* World = GetWorld();
	APlayerController* PlayerController = World
		? World->GetFirstPlayerController()
		: nullptr;

	if (IsValid(Player) &&
		Player->GetWorld() == World &&
		IsValid(PlayerController) &&
		PlayerController->GetPawn() == Player)
	{
		TryResolveMatch(EAshenOathMatchOutcome::Defeat);
	}
}

void AAshenOathGameMode::ReportBossDeath(
	AAshenOathBossCharacter* Boss)
{
	if (IsValid(Boss) && ActiveBoss.Get() == Boss)
	{
		TryResolveMatch(EAshenOathMatchOutcome::Victory);
	}
}

bool AAshenOathGameMode::TryResolveMatch(
	const EAshenOathMatchOutcome RequestedOutcome)
{
	if (RequestedOutcome == EAshenOathMatchOutcome::InProgress ||
		MatchOutcome != EAshenOathMatchOutcome::InProgress)
	{
		return false;
	}

	// Commit before broadcasting because listeners execute synchronously.
	MatchOutcome = RequestedOutcome;
	MatchOutcomeChangedEvent.Broadcast(MatchOutcome);

	return true;
}

bool AAshenOathGameMode::RequestRetry(
	APlayerController* RequestingController)
{
	UWorld* World = GetWorld();

	if (bRetryRequested ||
		MatchOutcome == EAshenOathMatchOutcome::InProgress ||
		!World ||
		!IsValid(RequestingController) ||
		!RequestingController->IsLocalController() ||
		World->GetFirstPlayerController() != RequestingController)
	{
		return false;
	}

	bRetryRequested = true;

	if (!ReloadCurrentLevel())
	{
		bRetryRequested = false;
		return false;
	}

	return true;
}

bool AAshenOathGameMode::ReloadCurrentLevel()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const FString CurrentLevelName =
		UGameplayStatics::GetCurrentLevelName(this, true);
	if (CurrentLevelName.IsEmpty())
	{
		return false;
	}

	UGameplayStatics::OpenLevel(this, FName(*CurrentLevelName));
	return true;
}
