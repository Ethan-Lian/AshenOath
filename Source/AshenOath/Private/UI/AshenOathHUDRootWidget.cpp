#include "UI/AshenOathHUDRootWidget.h"

#include "UI/AshenOathBossStatusWidget.h"

void UAshenOathHUDRootWidget::SetActiveBoss(AAshenOathBossCharacter* Boss)
{
	if (BossStatus)
	{
		BossStatus->ObserveBoss(Boss);
	}
}
