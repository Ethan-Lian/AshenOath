#include "UI/AshenOathHUDRootWidget.h"

#include "UI/AshenOathBossStatusWidget.h"
#include "UI/AshenOathPlayerStatusWidget.h"

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
