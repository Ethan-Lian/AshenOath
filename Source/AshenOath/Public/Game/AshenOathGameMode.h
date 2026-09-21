#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "AshenOathGameMode.generated.h"

class AAshenOathBossCharacter;
class AAshenOathPlayerCharacter;
class APlayerController;

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

	// Accepts only the local controller, and only once the match has resolved,
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

private:
	bool TryResolveMatch(EAshenOathMatchOutcome RequestedOutcome);
	bool ReloadCurrentLevel();

	UPROPERTY(Transient)
	TWeakObjectPtr<AAshenOathBossCharacter> ActiveBoss;

	FActiveBossChangedEvent ActiveBossChangedEvent;
	FMatchOutcomeChangedEvent MatchOutcomeChangedEvent;

	// The first valid terminal report wins and is never overwritten.
	EAshenOathMatchOutcome MatchOutcome = EAshenOathMatchOutcome::InProgress;

	bool bRetryRequested = false;
};
