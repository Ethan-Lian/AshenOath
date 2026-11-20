#include "UI/AshenOathHUD.h"

#include "Characters/AshenOathPlayerCharacter.h"
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

	// The player belongs to this local controller. The snapshot covers the case
	// where possession happened before the HUD was created.
	BoundPlayerController = OwnerController;
	ActivePlayerHandle = OwnerController->GetOnNewPawnNotifier().AddUObject(
		this,
		&AAshenOathHUD::HandleActivePlayer
	);
	HandleActivePlayer(OwnerController->GetPawn());

	UWorld* World = GetWorld();

	AAshenOathGameMode* GameMode = World ? World->GetAuthGameMode<AAshenOathGameMode>() : nullptr;

	if (!GameMode)
	{
		RootWidget->SetActiveBoss(nullptr);
		return;
	}

	BoundGameMode = GameMode;

	ActiveBossChangedHandle = GameMode->OnActiveBossChanged().AddUObject(this, &AAshenOathHUD::HandleActiveBossChanged);
	MatchPresentationReadyHandle = GameMode->OnMatchPresentationReady().AddUObject(
		this,
		&AAshenOathHUD::HandleMatchPresentationReady
	);
	CombatStartedHandle = GameMode->OnCombatStarted().AddUObject(
		this,
		&AAshenOathHUD::HandleCombatStarted
	);
	IntroPlaybackChangedHandle = GameMode->OnIntroPlaybackChanged().AddUObject(
		this,
		&AAshenOathHUD::HandleIntroPlaybackChanged
	);

	// Initialize from a Boss that registered before the HUD.
	HandleIntroPlaybackChanged(GameMode->IsIntroPlaying());
	HandleActiveBossChanged(GameMode->GetActiveBoss());
	HandleMatchPresentationReady(GameMode->IsMatchPresentationReady()
		? GameMode->GetMatchOutcome() : EAshenOathMatchOutcome::InProgress);
}

void AAshenOathHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (APlayerController* PlayerController = BoundPlayerController.Get())
	{
		if (ActivePlayerHandle.IsValid())
		{
			PlayerController->GetOnNewPawnNotifier().Remove(ActivePlayerHandle);
		}
	}

	ActivePlayerHandle.Reset();
	BoundPlayerController.Reset();

	if (AAshenOathGameMode* GameMode = BoundGameMode.Get())
	{
		if (ActiveBossChangedHandle.IsValid())
		{
			GameMode->OnActiveBossChanged().Remove(ActiveBossChangedHandle);
		}

		if (MatchPresentationReadyHandle.IsValid())
		{
			GameMode->OnMatchPresentationReady().Remove(MatchPresentationReadyHandle);
		}

		if (CombatStartedHandle.IsValid())
		{
			GameMode->OnCombatStarted().Remove(CombatStartedHandle);
		}

		if (IntroPlaybackChangedHandle.IsValid())
		{
			GameMode->OnIntroPlaybackChanged().Remove(IntroPlaybackChangedHandle);
		}
	}

	ActiveBossChangedHandle.Reset();
	MatchPresentationReadyHandle.Reset();
	CombatStartedHandle.Reset();
	IntroPlaybackChangedHandle.Reset();
	BoundGameMode.Reset();

	if (RootWidget)
	{
		RootWidget->SetActivePlayer(nullptr);
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
		AAshenOathGameMode* GameMode = BoundGameMode.Get();
		RootWidget->SetActiveBoss(GameMode && GameMode->IsCombatActive() ? BossCharacter : nullptr);
	}
}

void AAshenOathHUD::HandleCombatStarted()
{
	if (AAshenOathGameMode* GameMode = BoundGameMode.Get())
	{
		HandleActiveBossChanged(GameMode->GetActiveBoss());
	}
}

void AAshenOathHUD::HandleActivePlayer(APawn* NewPawn)
{
	if (RootWidget)
	{
		RootWidget->SetActivePlayer(Cast<AAshenOathPlayerCharacter>(NewPawn));
	}
}

void AAshenOathHUD::HandleIntroPlaybackChanged(const bool bPlaying)
{
	if (RootWidget)
	{
		RootWidget->SetIntroPlaying(bPlaying);
	}
}

void AAshenOathHUD::HandleMatchPresentationReady(
	const EAshenOathMatchOutcome Outcome)
{
	if (RootWidget)
	{
		RootWidget->SetMatchOutcome(Outcome);
	}
}
