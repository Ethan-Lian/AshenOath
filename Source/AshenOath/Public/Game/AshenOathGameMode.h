#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "AshenOathGameMode.generated.h"

class AAshenOathBossCharacter;

/*
 * defines the game rules and tracks the currently active Boss.
 * 
 * Stores a non-owning reference to the active boss, and broadcasts change
 * so gameplay and UI systems can react without directly depending on each other. 
 */

/*
Boss
 ↓ Register / Unregister
GameMode
 ↓ updates ActiveBoss
 ↓ broadcasts event
Gameplay / HUD / UI listeners
*/
UCLASS()
class ASHENOATH_API AAshenOathGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	AAshenOathGameMode();

	// Consumers can subscribe through OnActiveBossChanged, but only this GameMode can broadcast the event.
	DECLARE_EVENT_OneParam(AAshenOathGameMode, FActiveBossChangedEvent, AAshenOathBossCharacter*);

	// Registers a Boss after its gameplay setup and attributes are ready,
	// then notifies subscribers that it has become the active Boss.
	bool RegisterBoss(AAshenOathBossCharacter* Boss);
	
	// Unregisters a Boss leaving the gameplay lifecycle
	// and notifies subscribers that the active Boss has changed.
	void UnregisterBoss(AAshenOathBossCharacter* Boss);

	AAshenOathBossCharacter* GetActiveBoss() const;

	// Provides access to the event so external systems can subscribe to Boss changes.
	FActiveBossChangedEvent& OnActiveBossChanged()
	{
		return ActiveBossChangedEvent;
	}

private:
	// Non-owning reference to the currently active Boss.
	UPROPERTY(Transient)
	TWeakObjectPtr<AAshenOathBossCharacter> ActiveBoss;

	// Broadcasts active Boss changes to the systems such as the HUD.
	FActiveBossChangedEvent ActiveBossChangedEvent;
};
