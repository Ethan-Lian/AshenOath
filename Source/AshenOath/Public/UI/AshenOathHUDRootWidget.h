#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AshenOathHUDRootWidget.generated.h"

class AAshenOathPlayerCharacter;
class UAshenOathPlayerStatusWidget;
class AAshenOathBossCharacter;
class UAshenOathBossStatusWidget;

/**
 * Root UMG container owned by the HUD.
 *
 * The HUD calls root widget, while the root organizes
 * and Passes UI updates from the HUD to the appropriate child widgets.
 */

UCLASS(Abstract, Blueprintable)
class ASHENOATH_API UAshenOathHUDRootWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetActiveBoss(AAshenOathBossCharacter* Boss);

	void SetActivePlayer(AAshenOathPlayerCharacter* Player);

protected:
	// Required child supplied by WBP_HUDRoot under the name BossStatus.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UAshenOathBossStatusWidget> BossStatus;

	// Required child supplied by WBP_HUDRoot under the name PlayerStatus.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UAshenOathPlayerStatusWidget> PlayerStatus;
};
