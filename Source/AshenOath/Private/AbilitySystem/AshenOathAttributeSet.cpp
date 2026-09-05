#include "AbilitySystem/AshenOathAttributeSet.h"

#include "GameplayEffectExtension.h"

void UAshenOathAttributeSet::PreAttributeChange(
	const FGameplayAttribute& Attribute,
	float& NewValue
)
{
	Super::PreAttributeChange(Attribute, NewValue);

	// Clamp maxima first so the dependent attributes always receive a valid bound.
	if (Attribute == GetMaxHealthAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
	else if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(
			NewValue,
			0.0f,
			FMath::Max(GetMaxHealth(), 0.0f)
		);
	}
	else if (Attribute == GetMaxStaminaAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
	else if (Attribute == GetStaminaAttribute())
	{
		NewValue = FMath::Clamp(
			NewValue,
			0.0f,
			FMath::Max(GetMaxStamina(), 0.0f)
		);
	}
}

void UAshenOathAttributeSet::PostAttributeChange(
	const FGameplayAttribute& Attribute,
	float OldValue,
	float NewValue
)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	// If a new maximum falls below the current value, clamp the current value to that maximum.
	// Raising the maximum leaves the current value unchanged (no automatic refill).
	if (Attribute == GetMaxHealthAttribute() && GetHealth() > NewValue)
	{
		SetHealth(FMath::Max(NewValue, 0.0f));
	}
	else if (Attribute == GetMaxStaminaAttribute() && GetStamina() > NewValue)
	{
		SetStamina(FMath::Max(NewValue, 0.0f));
	}
}

void UAshenOathAttributeSet::PostGameplayEffectExecute(
	const FGameplayEffectModCallbackData& Data
)
{
	Super::PostGameplayEffectExecute(Data);

	// EvaluatedData identifies the attribute actually produced by the executed modifier.
	// Read the stored value again because the effect has already committed at this point.
	const FGameplayAttribute& ModifiedAttribute =
		Data.EvaluatedData.Attribute;

	if (ModifiedAttribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(
			GetHealth(),
			0.0f,
			FMath::Max(GetMaxHealth(), 0.0f)
		));
	}
	else if (ModifiedAttribute == GetMaxHealthAttribute())
	{
		SetMaxHealth(FMath::Max(GetMaxHealth(), 0.0f));
		SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
	}
	else if (ModifiedAttribute == GetStaminaAttribute())
	{
		SetStamina(FMath::Clamp(
			GetStamina(),
			0.0f,
			FMath::Max(GetMaxStamina(), 0.0f)
		));
	}
	else if (ModifiedAttribute == GetMaxStaminaAttribute())
	{
		SetMaxStamina(FMath::Max(GetMaxStamina(), 0.0f));
		SetStamina(FMath::Clamp(GetStamina(), 0.0f, GetMaxStamina()));
	}
}


