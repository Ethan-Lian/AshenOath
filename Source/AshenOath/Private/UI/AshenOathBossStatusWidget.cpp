#include "UI/AshenOathBossStatusWidget.h"

#include "AbilitySystem/AshenOathAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Characters/AshenOathBossCharacter.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UAshenOathBossStatusWidget::ObserveBoss(AAshenOathBossCharacter* Boss)
{
	// Always detach the previous source first. This also makes repeated calls safe.
	UnbindAttributes();

	if (!IsValid(Boss))
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	UAbilitySystemComponent* AbilitySystemComponent = Boss->GetAbilitySystemComponent();

	if (!IsValid(AbilitySystemComponent))
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	ObservedAbilitySystem = AbilitySystemComponent;

	HealthChangedHandle =
	    AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UAshenOathAttributeSet::GetHealthAttribute())
	        .AddUObject(this, &UAshenOathBossStatusWidget::HandleAttributeChanged);

	MaxHealthChangedHandle =
	    AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UAshenOathAttributeSet::GetMaxHealthAttribute())
	        .AddUObject(this, &UAshenOathBossStatusWidget::HandleAttributeChanged);

	SetVisibility(ESlateVisibility::HitTestInvisible);
	RefreshHealth();
}

void UAshenOathBossStatusWidget::NativeDestruct()
{
	UnbindAttributes();

	Super::NativeDestruct();
}

void UAshenOathBossStatusWidget::HandleAttributeChanged(const FOnAttributeChangeData&)
{
	RefreshHealth();
}

void UAshenOathBossStatusWidget::RefreshHealth()
{
	UAbilitySystemComponent* AbilitySystem = ObservedAbilitySystem.Get();

	if (!IsValid(AbilitySystem) || !HealthBar)
	{
		return;
	}

	const float Health = AbilitySystem->GetNumericAttribute(UAshenOathAttributeSet::GetHealthAttribute());
	const float MaxHealth = AbilitySystem->GetNumericAttribute(UAshenOathAttributeSet::GetMaxHealthAttribute());
	const float HealthPercent = MaxHealth > 0.0f ? FMath::Clamp(Health / MaxHealth, 0.0f, 1.0f) : 0.0f;

	HealthBar->SetPercent(HealthPercent);

	if (HealthText)
	{
		HealthText->SetText(FText::Format(NSLOCTEXT("AshenOathUI", "BossHealthFormat", "{0} / {1}"),
		                                  FText::AsNumber(FMath::RoundToInt(Health)),
		                                  FText::AsNumber(FMath::RoundToInt(MaxHealth))));
	}
}

void UAshenOathBossStatusWidget::UnbindAttributes()
{
	if (UAbilitySystemComponent* AbilitySystem = ObservedAbilitySystem.Get())
	{
		if (HealthChangedHandle.IsValid())
		{
			AbilitySystem->GetGameplayAttributeValueChangeDelegate(UAshenOathAttributeSet::GetHealthAttribute())
			    .Remove(HealthChangedHandle);
		}

		if (MaxHealthChangedHandle.IsValid())
		{
			AbilitySystem->GetGameplayAttributeValueChangeDelegate(UAshenOathAttributeSet::GetMaxHealthAttribute())
			    .Remove(MaxHealthChangedHandle);
		}
	}

	HealthChangedHandle.Reset();
	MaxHealthChangedHandle.Reset();
	ObservedAbilitySystem.Reset();
}
