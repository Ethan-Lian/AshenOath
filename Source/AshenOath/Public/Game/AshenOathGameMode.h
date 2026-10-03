#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "TimerManager.h"
#include "AshenOathGameMode.generated.h"

class AAshenOathBossCharacter;
class AAshenOathPlayerCharacter;
class APlayerController;
class ULevelSequence;
class ULevelSequencePlayer;
class ALevelSequenceActor;
class UAudioComponent;
class USoundBase;
class USoundMix;

UENUM(BlueprintType)
enum class EAshenOathMatchOutcome : uint8
{
	InProgress,
	Victory,
	Defeat
};

/**
 * Tracks the active Boss and broadcasts changes to interested systems.
 */

UCLASS()
class ASHENOATH_API AAshenOathGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	AAshenOathGameMode();

	DECLARE_EVENT_OneParam(AAshenOathGameMode, FActiveBossChangedEvent, AAshenOathBossCharacter*);
	DECLARE_EVENT_OneParam(AAshenOathGameMode, FMatchOutcomeChangedEvent, EAshenOathMatchOutcome);
	DECLARE_EVENT_OneParam(AAshenOathGameMode, FMatchPresentationReadyEvent, EAshenOathMatchOutcome);
	DECLARE_EVENT(AAshenOathGameMode, FCombatStartedEvent);
	DECLARE_EVENT_OneParam(AAshenOathGameMode, FIntroPlaybackChangedEvent, bool);

	// Registers a Boss after its gameplay setup and attributes are ready,
	// then notifies subscribers that it has become the active Boss.
	bool RegisterBoss(AAshenOathBossCharacter* Boss);

	// Unregisters a Boss leaving the gameplay lifecycle
	// and notifies subscribers that the active Boss has changed.
	void UnregisterBoss(AAshenOathBossCharacter* Boss);

	AAshenOathBossCharacter* GetActiveBoss() const;
	EAshenOathMatchOutcome GetMatchOutcome() const;

	void ReportPlayerDeath(AAshenOathPlayerCharacter* Player);
	void ReportBossDeath(AAshenOathBossCharacter* Boss);
	void ReportDeathPresentationFinished(AActor* DeadActor);
	bool IsMatchPresentationReady() const { return bMatchPresentationReady; }

	// Accepts only the local controller, and only once the result UI is ready,
	// then reloads the current level. Returns whether the retry was accepted.
	bool RequestRetry(APlayerController* RequestingController);

	FActiveBossChangedEvent& OnActiveBossChanged()
	{
		return ActiveBossChangedEvent;
	}

	FMatchOutcomeChangedEvent& OnMatchOutcomeChanged()
	{
		return MatchOutcomeChangedEvent;
	}

	FMatchPresentationReadyEvent& OnMatchPresentationReady()
	{
		return MatchPresentationReadyEvent;
	}

	FCombatStartedEvent& OnCombatStarted()
	{
		return CombatStartedEvent;
	}

	FIntroPlaybackChangedEvent& OnIntroPlaybackChanged()
	{
		return IntroPlaybackChangedEvent;
	}

	bool IsIntroPlaying() const
	{
		return IntroPlayer != nullptr;
	}

	// Called by the arena entrance trigger for the currently controlled player.
	UFUNCTION(BlueprintCallable, Category = "AshenOath|Intro")
	bool TryStartBossIntro(AAshenOathPlayerCharacter* EnteringPlayer);

	bool IsCombatActive() const
	{
		return bCombatActive && MatchOutcome == EAshenOathMatchOutcome::InProgress;
	}

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool TryResolveMatch(EAshenOathMatchOutcome RequestedOutcome, AActor* DeadActor);
	void ShowMatchPresentation();
	bool ReloadCurrentLevel();

	UPROPERTY(Transient)
	TWeakObjectPtr<AAshenOathBossCharacter> ActiveBoss;

	FActiveBossChangedEvent ActiveBossChangedEvent;
	FMatchOutcomeChangedEvent MatchOutcomeChangedEvent;
	FMatchPresentationReadyEvent MatchPresentationReadyEvent;
	FCombatStartedEvent CombatStartedEvent;
	FIntroPlaybackChangedEvent IntroPlaybackChangedEvent;

	// The first valid terminal report wins and is never overwritten.
	EAshenOathMatchOutcome MatchOutcome = EAshenOathMatchOutcome::InProgress;
	TWeakObjectPtr<AActor> MatchEndingActor;
	bool bDeathPresentationReported = false;
	bool bMatchPresentationReady = false;
	FTimerHandle MatchPresentationTimer;

	// Keeps death effects visible briefly after the Montage ends or cannot play.
	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Match", meta = (ClampMin = "0.0", Units = "s"))
	float DeathPresentationHoldSeconds = 0.5f;

	bool bRetryRequested = false;

	void BeginCombat();
	void StartCombatMusic();
	void ClearCombatMusic();

	// Looping is configured on the sound asset; this music plays without spatialization.
	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Music")
	TObjectPtr<USoundBase> CombatMusic;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Music", meta = (ClampMin = "0.0", UIMax = "1.0"))
	float CombatMusicVolume = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Music", meta = (ClampMin = "0.0", Units = "s"))
	float CombatMusicFadeInSeconds = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Music", meta = (ClampMin = "0.0", Units = "s"))
	float CombatMusicFadeOutSeconds = 0.5f;

	// GameMode retains the component through result presentation and releases it on exit.
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> CombatMusicComponent;

	void ClearIntroPlayback();

	UFUNCTION()
	void HandleIntroFinished();

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Intro")
	TObjectPtr<ULevelSequence> IntroSequence;

	// Only adjusts the environment sound class; cinematic audio uses a separate class.
	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Intro")
	TObjectPtr<USoundMix> IntroSoundMix;

	UPROPERTY(Transient)
	TObjectPtr<ULevelSequencePlayer> IntroPlayer;

	UPROPERTY(Transient)
	TObjectPtr<ALevelSequenceActor> IntroActor;

	bool bIntroSoundMixPushed = false;

	bool bIntroTriggered = false;

	bool bCombatActive = false;
};
