#include "AbilitySystem/Data/AshenOathHeavyAttackData.h"

#include "AbilitySystem/AshenOathChargedThrustDamageEffect.h"
#include "AbilitySystem/AshenOathHeavyChargeCostEffect.h"

UAshenOathHeavyAttackData::UAshenOathHeavyAttackData()
{
	CostEffect = nullptr;
	ChargeCostEffect = UAshenOathHeavyChargeCostEffect::StaticClass();
	DamageEffect = UAshenOathChargedThrustDamageEffect::StaticClass();
}
