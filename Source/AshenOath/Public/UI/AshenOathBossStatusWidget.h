#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AshenOathBossStatusWidget.generated.h"

class AAshenOathBossCharacter;
class UAbilitySystemComponent;
class UProgressBar;
class UTextBlock;
struct FOnAttributeChangeData;

/**
 * Displays the current Boss health status.
 *
 * Observes the Boss AbilitySystemComponent for health changes
 * and updates the bound UI widgets accordingly.
 */
UCLASS(Abstract, Blueprintable)
class ASHENOATH_API UAshenOathBossStatusWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Entry point for observing a Boss. Binds attribute-change delegates; nullptr stops observation.
	void ObserveBoss(AAshenOathBossCharacter* Boss);

protected:
	virtual void NativeDestruct() override;

	// meta = (BindWidget) means the Blueprint child must contain a ProgressBar named HealthBar.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HealthBar;

	// The health bar still works if the optional HealthText widget is absent.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HealthText;

private:
	// Callback when attribute changed
	void HandleAttributeChanged(const FOnAttributeChangeData& ChangeData);

	void RefreshHealth();

	void UnbindAttributes();

	// The widget observes the ASC without owning it; the Boss may disappear first.
	TWeakObjectPtr<UAbilitySystemComponent> ObservedAbilitySystem;

	FDelegateHandle HealthChangedHandle;

	FDelegateHandle MaxHealthChangedHandle;
};
