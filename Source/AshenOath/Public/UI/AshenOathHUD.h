#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "AshenOathHUD.generated.h"

class AAshenOathPlayerCharacter;
class AAshenOathGameMode;
class UAshenOathHUDRootWidget;
class AAshenOathBossCharacter;
class APlayerController;
class APawn;


/**
 * Player-facing UI coordinator.
 *
 * Gameplay defines the authoritative data.
 * HUD decides which UI should respond to gameplay state/events.
 * Widgets handle how that data is presented.
 *
 * HUD Owns the root HUD widget and bridges gameplay systems to UMG.
 */
UCLASS()
class ASHENOATH_API AAshenOathHUD : public AHUD
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleActiveBossChanged(AAshenOathBossCharacter* BossCharacter);

	void HandleActivePlayer(APawn* NewPawn);

	// Root widget class created by this HUD at runtime.
	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|UI")
	TSubclassOf<UAshenOathHUDRootWidget> RootWidgetClass;

	// HUD creates and owns RootWidget instance for its active lifetime.
	UPROPERTY(Transient)
	TObjectPtr<UAshenOathHUDRootWidget> RootWidget;
	
	// The World owns GameMode. HUD only observes the exact instance from which it registered the delegate.
	TWeakObjectPtr<AAshenOathGameMode> BoundGameMode;

	// Pawn-change cleanup must use the same controller that supplied the delegate.
	TWeakObjectPtr<APlayerController> BoundPlayerController;

	FDelegateHandle ActiveBossChangedHandle;

	FDelegateHandle ActivePlayerHandle;
};
