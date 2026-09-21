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
enum class EAshenOathMatchOutcome : uint8;


/**
 * Owns the root HUD widget and forwards gameplay state to UMG.
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
	void HandleMatchOutcomeChanged(EAshenOathMatchOutcome Outcome);

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|UI")
	TSubclassOf<UAshenOathHUDRootWidget> RootWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UAshenOathHUDRootWidget> RootWidget;
	
	// The World owns GameMode. HUD only observes the exact instance from which it registered the delegate.
	TWeakObjectPtr<AAshenOathGameMode> BoundGameMode;

	// Pawn-change cleanup must use the same controller that supplied the delegate.
	TWeakObjectPtr<APlayerController> BoundPlayerController;

	FDelegateHandle ActiveBossChangedHandle;
	FDelegateHandle MatchOutcomeChangedHandle;

	FDelegateHandle ActivePlayerHandle;
};
