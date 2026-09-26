#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AshenOathPlayerStatusWidget.generated.h"

struct FOnAttributeChangeData;
class UProgressBar;
class UTextBlock;
class UAbilitySystemComponent;
class AAshenOathPlayerCharacter;

UCLASS(Abstract, Blueprintable)
class ASHENOATH_API UAshenOathPlayerStatusWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Entry point for observing a player. Binds attribute-change delegates; nullptr stops observation.
	void ObservePlayer(AAshenOathPlayerCharacter* Player);

protected:
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HealthBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HealthText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> StaminaBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StaminaText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HealUsesText;

private:
	void HandleAttributeChanged(const FOnAttributeChangeData& ChangeData);

	void RefreshHealth();

	void RefreshStamina();
	void RefreshHealUses();
	void HandleHealUsesChanged(int32 NewRemainingUses);

	void UnbindAttributes();

	// The widget observes the ASC without owning it; the player may disappear first.
	TWeakObjectPtr<UAbilitySystemComponent> ObservedAbilitySystem;
	TWeakObjectPtr<AAshenOathPlayerCharacter> ObservedPlayer;

	FDelegateHandle HealthChangedHandle;

	FDelegateHandle MaxHealthChangedHandle;

	FDelegateHandle StaminaChangedHandle;

	FDelegateHandle MaxStaminaChangedHandle;
	FDelegateHandle HealUsesChangedHandle;
};
