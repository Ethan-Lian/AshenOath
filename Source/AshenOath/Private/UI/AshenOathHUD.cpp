#include "UI/AshenOathHUD.h"

#include "Game/AshenOathGameMode.h"
#include "GameFramework/PlayerController.h"
#include "UI/AshenOathHUDRootWidget.h"

void AAshenOathHUD::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* OwnerController = GetOwningPlayerController();

	if (!IsValid(OwnerController) || !OwnerController->IsLocalController() || !RootWidgetClass)
	{
		return;
	}

	RootWidget = CreateWidget<UAshenOathHUDRootWidget>(OwnerController, RootWidgetClass);

	if (!RootWidget)
	{
		return;
	}

	RootWidget->AddToPlayerScreen();

	UWorld* World = GetWorld();

	AAshenOathGameMode* GameMode = World ? World->GetAuthGameMode<AAshenOathGameMode>() : nullptr;

	if (!GameMode)
	{
		RootWidget->SetActiveBoss(nullptr);
		return;
	}

	BoundGameMode = GameMode;

	ActiveBossChangedHandle = GameMode->OnActiveBossChanged().AddUObject(this, &AAshenOathHUD::HandleActiveBossChanged);

	/*
	 * The event handles future changes.
	 * This snapshot handles a Boss that registered before the HUD.
	 */
	HandleActiveBossChanged(GameMode->GetActiveBoss());
}

void AAshenOathHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AAshenOathGameMode* GameMode = BoundGameMode.Get())
	{
		if (ActiveBossChangedHandle.IsValid())
		{
			GameMode->OnActiveBossChanged().Remove(ActiveBossChangedHandle);
		}
	}

	ActiveBossChangedHandle.Reset();
	BoundGameMode.Reset();

	// Detach the widget from the screen, then release HUD's strong reference.
	// Unreal's garbage collector destroys the UObject later when it is no longer referenced.
	if (RootWidget)
	{
		RootWidget->SetActiveBoss(nullptr);
		RootWidget->RemoveFromParent();
		RootWidget = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void AAshenOathHUD::HandleActiveBossChanged(AAshenOathBossCharacter* BossCharacter)
{
	if (RootWidget)
	{
		RootWidget->SetActiveBoss(BossCharacter);
	}
}
