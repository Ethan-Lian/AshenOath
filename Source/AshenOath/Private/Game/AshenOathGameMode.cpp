#include "Game/AshenOathGameMode.h"

#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieSceneSequencePlaybackSettings.h"
#include "Characters/AshenOathBossCharacter.h"
#include "Characters/AshenOathPlayerCharacter.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
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

void AAshenOathGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearIntroPlayback();
	Super::EndPlay(EndPlayReason);
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
	const bool bIntroWasPlaying = IntroPlayer != nullptr;
	if (bIntroWasPlaying)
	{
		ClearIntroPlayback();
	}

	if (UWorld* World = GetWorld())
	{
		if (AAshenOathPlayerController* PlayerController =
			Cast<AAshenOathPlayerController>(World->GetFirstPlayerController()))
		{
			if (bIntroWasPlaying && IsValid(PlayerController->GetPawn()))
			{
				PlayerController->SetViewTarget(PlayerController->GetPawn());
			}
			PlayerController->SetGameplayInputEnabled(false);
		}
	}

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

bool AAshenOathGameMode::TryStartBossIntro(
	AAshenOathPlayerCharacter* EnteringPlayer)
{
	UWorld* World = GetWorld();
	if (bIntroTriggered ||
		MatchOutcome != EAshenOathMatchOutcome::InProgress ||
		!World ||
		!IsValid(EnteringPlayer) ||
		EnteringPlayer->GetWorld() != World ||
		!IsValid(ActiveBoss.Get()))
	{
		return false;
	}

	AAshenOathPlayerController* PlayerController =
		Cast<AAshenOathPlayerController>(World->GetFirstPlayerController());

	if (!IsValid(PlayerController) ||
		!PlayerController->IsLocalController() ||
		PlayerController->GetPawn() != EnteringPlayer)
	{
		return false;
	}

	// Claim this intro before calls that may synchronously invoke gameplay callbacks.
	bIntroTriggered = true;
	PlayerController->EnterCinematicInputMode();
	EnteringPlayer->PrepareForBossIntro();

	if (MatchOutcome != EAshenOathMatchOutcome::InProgress)
	{
		return true;
	}

	// Allow the entrance to start combat while no sequence asset is configured.
	if (!IntroSequence)
	{
		BeginCombat();
		return true;
	}

	FMovieSceneSequencePlaybackSettings PlaybackSettings;
	ALevelSequenceActor* CreatedActor = nullptr;
	IntroPlayer = ULevelSequencePlayer::CreateLevelSequencePlayer(
		this, IntroSequence.Get(), PlaybackSettings, CreatedActor
	);
	IntroActor = CreatedActor;

	if (!IntroPlayer)
	{
		BeginCombat();
		return true;
	}

	IntroPlayer->OnFinished.AddDynamic(
		this, &AAshenOathGameMode::HandleIntroFinished
	);
	IntroPlayer->Play();
	return true;
}

void AAshenOathGameMode::BeginCombat()
{
	if (!bIntroTriggered ||
		bCombatActive ||
		MatchOutcome != EAshenOathMatchOutcome::InProgress)
	{
		return;
	}

	UWorld* World = GetWorld();
	AAshenOathBossCharacter* Boss = ActiveBoss.Get();
	AAshenOathPlayerController* PlayerController = World
		? Cast<AAshenOathPlayerController>(World->GetFirstPlayerController())
		: nullptr;

	if (!IsValid(Boss) ||
		!IsValid(PlayerController) ||
		!IsValid(PlayerController->GetPawn()))
	{
		return;
	}

	ClearIntroPlayback();
	PlayerController->SetViewTarget(PlayerController->GetPawn());

	bCombatActive = true;
	PlayerController->SetGameplayInputEnabled(true);
	Boss->StartCombatLogic();

	if (IsCombatActive())
	{
		CombatStartedEvent.Broadcast();
	}
}

void AAshenOathGameMode::ClearIntroPlayback()
{
	if (IntroPlayer)
	{
		IntroPlayer->OnFinished.RemoveDynamic(this, &AAshenOathGameMode::HandleIntroFinished);
		IntroPlayer->Stop();
		IntroPlayer = nullptr;
	}

	if (IsValid(IntroActor))
	{
		IntroActor->Destroy();
	}
	IntroActor = nullptr;
}

void AAshenOathGameMode::HandleIntroFinished()
{
	BeginCombat();
}
