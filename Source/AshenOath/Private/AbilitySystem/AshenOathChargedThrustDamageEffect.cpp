#include "AbilitySystem/AshenOathChargedThrustDamageEffect.h"

#include "AbilitySystem/AshenOathAttributeSet.h"
#include "GameplayTags/AshenOathGameplayTags.h"

UAshenOathChargedThrustDamageEffect::UAshenOathChargedThrustDamageEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FSetByCallerFloat DamageMagnitude;
	DamageMagnitude.DataTag = AshenOathGameplayTags::Data_Damage;

	FGameplayModifierInfo& HealthModifier = Modifiers.AddDefaulted_GetRef();
	HealthModifier.Attribute = UAshenOathAttributeSet::GetHealthAttribute();
	HealthModifier.ModifierOp = EGameplayModOp::AddBase;
	HealthModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(
		DamageMagnitude
	);
}
