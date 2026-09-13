#include "AbilitySystem/AshenOathStaminaRegenerationEffect.h"

#include "AbilitySystem/AshenOathAttributeSet.h"

UAshenOathStaminaRegenerationEffect::UAshenOathStaminaRegenerationEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	Period = FScalableFloat(0.1f);
	bExecutePeriodicEffectOnApplication = false;

	FGameplayModifierInfo& StaminaModifier = Modifiers.AddDefaulted_GetRef();
	StaminaModifier.Attribute = UAshenOathAttributeSet::GetStaminaAttribute();
	StaminaModifier.ModifierOp = EGameplayModOp::AddBase;
	StaminaModifier.ModifierMagnitude = FScalableFloat(2.0f);
}
