#pragma once
#include "NativeGameplayTags.h"

namespace AshenOathGameplayTags
{
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Action);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Action_ComboAttack);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Action_HeavyAttack);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Action_Heal);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Action_Dodge);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Action_BossSingleSwing);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Action_BossCombo);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Action_BossChargedSwing);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Action_BossDashSwing);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Cost_Stamina);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Damage);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Invulnerable);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_MovementLocked);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Staggered);
}
