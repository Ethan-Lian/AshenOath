#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AshenOathHUDRootWidget.generated.h"

class AAshenOathPlayerCharacter;
class UAshenOathPlayerStatusWidget;
class AAshenOathBossCharacter;
class UAshenOathBossStatusWidget;
class UButton;
class UTextBlock;
class UWidget;
enum class EAshenOathMatchOutcome : uint8;

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
	void SetMatchOutcome(EAshenOathMatchOutcome Outcome);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UAshenOathBossStatusWidget> BossStatus;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UAshenOathPlayerStatusWidget> PlayerStatus;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> OutcomePanel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> OutcomeText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> RetryButton;

private:
	UFUNCTION()
	void HandleRetryClicked();
};
