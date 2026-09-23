#include "AbilitySystem/AshenOathHeavyChargeCostEffect.h"

#include "AbilitySystem/AshenOathAttributeSet.h"
#include "GameplayTags/AshenOathGameplayTags.h"

UAshenOathHeavyChargeCostEffect::UAshenOathHeavyChargeCostEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FSetByCallerFloat ChargeCostMagnitude;
	ChargeCostMagnitude.DataTag = AshenOathGameplayTags::Data_Cost_Stamina;

	FGameplayModifierInfo& StaminaModifier = Modifiers.AddDefaulted_GetRef();
	StaminaModifier.Attribute = UAshenOathAttributeSet::GetStaminaAttribute();
	StaminaModifier.ModifierOp = EGameplayModOp::AddBase;
	StaminaModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(
		ChargeCostMagnitude
	);
}
