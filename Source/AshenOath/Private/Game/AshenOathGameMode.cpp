#include "Game/AshenOathGameMode.h"

#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieSceneSequencePlaybackSettings.h"
#include "Characters/AshenOathBossCharacter.h"
#include "Characters/AshenOathPlayerCharacter.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Player/AshenOathPlayerController.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundMix.h"
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
		TryResolveMatch(EAshenOathMatchOutcome::Defeat, Player);
	}
}

void AAshenOathGameMode::ReportBossDeath(
	AAshenOathBossCharacter* Boss)
{
	if (IsValid(Boss) && ActiveBoss.Get() == Boss)
	{
		TryResolveMatch(EAshenOathMatchOutcome::Victory, Boss);
	}
}

void AAshenOathGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(MatchPresentationTimer);
	MatchEndingActor.Reset();
	ClearCombatMusic();
	ClearIntroPlayback();
	Super::EndPlay(EndPlayReason);
}

bool AAshenOathGameMode::TryResolveMatch(
	const EAshenOathMatchOutcome RequestedOutcome, AActor* DeadActor)
{
	if (RequestedOutcome == EAshenOathMatchOutcome::InProgress ||
		MatchOutcome != EAshenOathMatchOutcome::InProgress)
	{
		return false;
	}

	// Commit before broadcasting because listeners execute synchronously.
	MatchOutcome = RequestedOutcome;
	MatchEndingActor = DeadActor;
	if (CombatMusicComponent)
	{
		CombatMusicComponent->FadeOut(CombatMusicFadeOutSeconds, 0.0f);
	}

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
			PlayerController->EnterCinematicInputMode();
			if (RequestedOutcome == EAshenOathMatchOutcome::Victory)
			{
				if (AAshenOathPlayerCharacter* Player = Cast<AAshenOathPlayerCharacter>(PlayerController->GetPawn()))
				{
					Player->StopCombatForMatchEnd();
				}
			}
		}
	}
	if (RequestedOutcome == EAshenOathMatchOutcome::Defeat)
	{
		if (AAshenOathBossCharacter* Boss = ActiveBoss.Get())
		{
			Boss->StopCombatForMatchEnd();
		}
	}

	MatchOutcomeChangedEvent.Broadcast(MatchOutcome);

	return true;
}

void AAshenOathGameMode::ReportDeathPresentationFinished(AActor* DeadActor)
{
	if (!IsValid(DeadActor) || MatchEndingActor.Get() != DeadActor ||
		MatchOutcome == EAshenOathMatchOutcome::InProgress || bDeathPresentationReported)
	{
		return;
	}

	bDeathPresentationReported = true;
	if (DeathPresentationHoldSeconds > 0.0f)
	{
		GetWorldTimerManager().SetTimer(MatchPresentationTimer, this,
			&AAshenOathGameMode::ShowMatchPresentation, DeathPresentationHoldSeconds, false);
	}
	else
	{
		ShowMatchPresentation();
	}
}

void AAshenOathGameMode::ShowMatchPresentation()
{
	if (bMatchPresentationReady || !bDeathPresentationReported)
	{
		return;
	}

	bMatchPresentationReady = true;
	if (UWorld* World = GetWorld())
	{
		if (AAshenOathPlayerController* PlayerController =
			Cast<AAshenOathPlayerController>(World->GetFirstPlayerController()))
		{
			PlayerController->SetGameplayInputEnabled(false);
		}
	}
	MatchPresentationReadyEvent.Broadcast(MatchOutcome);
}

bool AAshenOathGameMode::RequestRetry(
	APlayerController* RequestingController)
{
	UWorld* World = GetWorld();

	if (bRetryRequested ||
		!bMatchPresentationReady ||
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
	IntroPlayer->OnStop.AddDynamic(
		this, &AAshenOathGameMode::HandleIntroFinished
	);

	if (IntroSoundMix)
	{
		bIntroSoundMixPushed = true;
		UGameplayStatics::PushSoundMixModifier(this, IntroSoundMix);
	}

	IntroPlaybackChangedEvent.Broadcast(true);
	// Presentation listeners can synchronously resolve the match and clear playback.
	if (!IntroPlayer || MatchOutcome != EAshenOathMatchOutcome::InProgress)
	{
		return true;
	}

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

	ClearIntroPlayback();
	if (MatchOutcome != EAshenOathMatchOutcome::InProgress)
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

	PlayerController->SetViewTarget(PlayerController->GetPawn());

	bCombatActive = true;
	PlayerController->SetGameplayInputEnabled(true);
	Boss->StartCombatLogic();

	if (IsCombatActive())
	{
		StartCombatMusic();
		CombatStartedEvent.Broadcast();
	}
}

void AAshenOathGameMode::StartCombatMusic()
{
	if (!CombatMusic)
	{
		return;
	}

	CombatMusicComponent = UGameplayStatics::CreateSound2D(
		this, CombatMusic, CombatMusicVolume, 1.0f, 0.0f, nullptr, false, false
	);
	if (CombatMusicComponent)
	{
		// Retain ownership before FadeIn starts playback and emits audio callbacks.
		CombatMusicComponent->FadeIn(CombatMusicFadeInSeconds);
	}
}

void AAshenOathGameMode::ClearCombatMusic()
{
	UAudioComponent* MusicToStop = CombatMusicComponent;
	CombatMusicComponent = nullptr;
	if (MusicToStop)
	{
		MusicToStop->Stop();
		MusicToStop->DestroyComponent();
	}
}

void AAshenOathGameMode::ClearIntroPlayback()
{
	ULevelSequencePlayer* PlayerToStop = IntroPlayer;
	ALevelSequenceActor* ActorToDestroy = IntroActor;
	IntroPlayer = nullptr;
	IntroActor = nullptr;

	if (PlayerToStop)
	{
		PlayerToStop->OnFinished.RemoveDynamic(this, &AAshenOathGameMode::HandleIntroFinished);
		PlayerToStop->OnStop.RemoveDynamic(this, &AAshenOathGameMode::HandleIntroFinished);
		PlayerToStop->Stop();
	}

	if (IsValid(ActorToDestroy))
	{
		ActorToDestroy->Destroy();
	}

	if (bIntroSoundMixPushed)
	{
		bIntroSoundMixPushed = false;
		UGameplayStatics::PopSoundMixModifier(this, IntroSoundMix);
	}

	if (PlayerToStop)
	{
		IntroPlaybackChangedEvent.Broadcast(false);
	}
}

void AAshenOathGameMode::HandleIntroFinished()
{
	BeginCombat();
}
