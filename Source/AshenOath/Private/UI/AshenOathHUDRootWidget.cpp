#include "UI/AshenOathHUDRootWidget.h"

#include "UI/AshenOathBossStatusWidget.h"
#include "UI/AshenOathPlayerStatusWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Game/AshenOathGameMode.h"
#include "Player/AshenOathPlayerController.h"

void UAshenOathHUDRootWidget::NativeConstruct()
{
	Super::NativeConstruct();

	RetryButton->OnClicked.AddUniqueDynamic(
		this,
		&UAshenOathHUDRootWidget::HandleRetryClicked
	);
}

void UAshenOathHUDRootWidget::NativeDestruct()
{
	RetryButton->OnClicked.RemoveDynamic(
		this,
		&UAshenOathHUDRootWidget::HandleRetryClicked
	);

	Super::NativeDestruct();
}

void UAshenOathHUDRootWidget::SetActiveBoss(AAshenOathBossCharacter* Boss)
{
	if (BossStatus)
	{
		BossStatus->ObserveBoss(Boss);
	}
}

void UAshenOathHUDRootWidget::SetActivePlayer(AAshenOathPlayerCharacter* Player)
{
	if (PlayerStatus)
	{
		PlayerStatus->ObservePlayer(Player);
	}
}

void UAshenOathHUDRootWidget::SetMatchOutcome(
	const EAshenOathMatchOutcome Outcome)
{
	RetryButton->SetIsEnabled(true);

	if (Outcome == EAshenOathMatchOutcome::InProgress)
	{
		OutcomePanel->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	const FText OutcomeLabel = Outcome == EAshenOathMatchOutcome::Victory
		? NSLOCTEXT("AshenOathUI", "VictoryOutcome", "VICTORY")
		: NSLOCTEXT("AshenOathUI", "DefeatOutcome", "DEFEAT");

	OutcomeText->SetText(OutcomeLabel);
	OutcomePanel->SetVisibility(ESlateVisibility::Visible);
}

void UAshenOathHUDRootWidget::HandleRetryClicked()
{
	AAshenOathPlayerController* PlayerController =
		Cast<AAshenOathPlayerController>(GetOwningPlayer());

	if (PlayerController && PlayerController->RequestRetry())
	{
		RetryButton->SetIsEnabled(false);
	}
}
