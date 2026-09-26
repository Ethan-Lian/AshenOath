#include "UI/AshenOathPlayerStatusWidget.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/AshenOathAttributeSet.h"
#include "Characters/AshenOathPlayerCharacter.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UAshenOathPlayerStatusWidget::ObservePlayer(AAshenOathPlayerCharacter* Player)
{
	// Always detach the previous source first. This also makes repeated calls safe.
	UnbindAttributes();

	if (!IsValid(Player))
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	UAbilitySystemComponent* AbilitySystemComponent = Player->GetAbilitySystemComponent();

	if (!IsValid(AbilitySystemComponent))
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	ObservedAbilitySystem = AbilitySystemComponent;
	ObservedPlayer = Player;
	HealUsesChangedHandle = Player->OnHealUsesChanged().AddUObject(
		this, &UAshenOathPlayerStatusWidget::HandleHealUsesChanged);

	HealthChangedHandle =
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UAshenOathAttributeSet::GetHealthAttribute())
			.AddUObject(this, &UAshenOathPlayerStatusWidget::HandleAttributeChanged);

	MaxHealthChangedHandle =
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UAshenOathAttributeSet::GetMaxHealthAttribute())
			.AddUObject(this, &UAshenOathPlayerStatusWidget::HandleAttributeChanged);

	StaminaChangedHandle =
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UAshenOathAttributeSet::GetStaminaAttribute())
			.AddUObject(this, &UAshenOathPlayerStatusWidget::HandleAttributeChanged);

	MaxStaminaChangedHandle =
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UAshenOathAttributeSet::GetMaxStaminaAttribute())
			.AddUObject(this, &UAshenOathPlayerStatusWidget::HandleAttributeChanged);

	SetVisibility(ESlateVisibility::HitTestInvisible);

	RefreshHealth();

	RefreshStamina();
	RefreshHealUses();
}

void UAshenOathPlayerStatusWidget::NativeDestruct()
{
	UnbindAttributes();

	Super::NativeDestruct();
}

void UAshenOathPlayerStatusWidget::HandleAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
	if (ChangeData.Attribute == UAshenOathAttributeSet::GetHealthAttribute() ||
		ChangeData.Attribute == UAshenOathAttributeSet::GetMaxHealthAttribute())
	{
		RefreshHealth();
	}
	else if (ChangeData.Attribute == UAshenOathAttributeSet::GetStaminaAttribute() ||
		ChangeData.Attribute == UAshenOathAttributeSet::GetMaxStaminaAttribute())
	{
		RefreshStamina();
	}
}

void UAshenOathPlayerStatusWidget::RefreshHealth()
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
		HealthText->SetText(FText::Format(NSLOCTEXT("AshenOathUI", "PlayerHealthFormat", "{0} / {1}"),
										  FText::AsNumber(FMath::RoundToInt(Health)),
										  FText::AsNumber(FMath::RoundToInt(MaxHealth))));
	}
}

void UAshenOathPlayerStatusWidget::RefreshStamina()
{
	UAbilitySystemComponent* AbilitySystem = ObservedAbilitySystem.Get();

	if (!IsValid(AbilitySystem) || !StaminaBar)
	{
		return;
	}

	const float Stamina = AbilitySystem->GetNumericAttribute(UAshenOathAttributeSet::GetStaminaAttribute());
	const float MaxStamina = AbilitySystem->GetNumericAttribute(UAshenOathAttributeSet::GetMaxStaminaAttribute());
	const float StaminaPercent = MaxStamina > 0.0f ? FMath::Clamp(Stamina / MaxStamina, 0.0f, 1.0f) : 0.0f;

	StaminaBar->SetPercent(StaminaPercent);

	if (StaminaText)
	{
		StaminaText->SetText(FText::Format(NSLOCTEXT("AshenOathUI", "PlayerStaminaFormat", "{0} / {1}"),
										  FText::AsNumber(FMath::RoundToInt(Stamina)),
										  FText::AsNumber(FMath::RoundToInt(MaxStamina))));
	}
}

void UAshenOathPlayerStatusWidget::RefreshHealUses()
{
	AAshenOathPlayerCharacter* Player = ObservedPlayer.Get();
	if (Player && HealUsesText)
	{
		HealUsesText->SetText(FText::Format(
			NSLOCTEXT("AshenOathUI", "PlayerHealUsesFormat", "Heals: {0}"),
			FText::AsNumber(Player->GetRemainingHealUses())
		));
	}
}

void UAshenOathPlayerStatusWidget::HandleHealUsesChanged(const int32)
{
	RefreshHealUses();
}

void UAshenOathPlayerStatusWidget::UnbindAttributes()
{
	if (AAshenOathPlayerCharacter* Player = ObservedPlayer.Get())
	{
		if (HealUsesChangedHandle.IsValid())
		{
			Player->OnHealUsesChanged().Remove(HealUsesChangedHandle);
		}
	}
	HealUsesChangedHandle.Reset();
	ObservedPlayer.Reset();

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

		if (StaminaChangedHandle.IsValid())
		{
			AbilitySystem->GetGameplayAttributeValueChangeDelegate(UAshenOathAttributeSet::GetStaminaAttribute())
				.Remove(StaminaChangedHandle);
		}

		if (MaxStaminaChangedHandle.IsValid())
		{
			AbilitySystem->GetGameplayAttributeValueChangeDelegate(UAshenOathAttributeSet::GetMaxStaminaAttribute())
				.Remove(MaxStaminaChangedHandle);
		}
	}

	HealthChangedHandle.Reset();
	MaxHealthChangedHandle.Reset();
	StaminaChangedHandle.Reset();
	MaxStaminaChangedHandle.Reset();
	ObservedAbilitySystem.Reset();
}
