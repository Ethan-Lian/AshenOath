#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AshenOathHUDRootWidget.generated.h"

class AAshenOathPlayerCharacter;
class UAshenOathPlayerStatusWidget;
class AAshenOathBossCharacter;
class UAshenOathBossStatusWidget;

/**
 * Routes HUD updates to the appropriate child widgets.
 */

UCLASS(Abstract, Blueprintable)
class ASHENOATH_API UAshenOathHUDRootWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetActiveBoss(AAshenOathBossCharacter* Boss);

	void SetActivePlayer(AAshenOathPlayerCharacter* Player);

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UAshenOathBossStatusWidget> BossStatus;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UAshenOathPlayerStatusWidget> PlayerStatus;
};
