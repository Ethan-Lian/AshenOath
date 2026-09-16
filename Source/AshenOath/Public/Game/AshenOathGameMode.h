#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "AshenOathGameMode.generated.h"

class AAshenOathBossCharacter;

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

	// Registers a Boss after its gameplay setup and attributes are ready,
	// then notifies subscribers that it has become the active Boss.
	bool RegisterBoss(AAshenOathBossCharacter* Boss);
	
	// Unregisters a Boss leaving the gameplay lifecycle
	// and notifies subscribers that the active Boss has changed.
	void UnregisterBoss(AAshenOathBossCharacter* Boss);

	AAshenOathBossCharacter* GetActiveBoss() const;

	FActiveBossChangedEvent& OnActiveBossChanged()
	{
		return ActiveBossChangedEvent;
	}

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<AAshenOathBossCharacter> ActiveBoss;

	FActiveBossChangedEvent ActiveBossChangedEvent;
};
