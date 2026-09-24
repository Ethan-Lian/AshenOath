#include "GameplayTags/AshenOathGameplayTags.h"

namespace AshenOathGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
	Ability_Action,
	"Ability.Action",
	"Classifies abilities that participate in combat-action mutual exclusion."
	);

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Action_ComboAttack,
		"Ability.Action.ComboAttack",
		"Identifies the player's combo-attack ability."
	);

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Action_HeavyAttack,
		"Ability.Action.HeavyAttack",
		"Identifies the player's heavy and charged-heavy ability."
	);

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Action_Dodge,
		"Ability.Action.Dodge",
		"Identifies a combat dodge ability."
	);

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Action_BossSingleSwing,
		"Ability.Action.BossSingleSwing",
		"Identifies the Boss single-swing combat ability."
	);

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Action_BossCombo,
		"Ability.Action.BossCombo",
		"Identifies the Boss combo combat ability."
	);

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Action_BossChargedSwing,
		"Ability.Action.BossChargedSwing",
		"Identifies the Boss charged-swing combat ability."
	);

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Action_BossDashSwing,
		"Ability.Action.BossDashSwing",
		"Identifies the Boss dash-swing combat ability."
	);

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Data_Cost_Stamina,
		"Data.Cost.Stamina",
		"SetByCaller magnitude for an action-authored Stamina change."
	);

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Data_Damage,
		"Data.Damage",
		"SetByCaller magnitude for an action-authored Health change."
	);

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		State_Dead,
		"State.Dead",
		"The combatant is dead and cannot accept gameplay actions."
	);

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		State_Invulnerable,
		"State.Invulnerable",
		"The combatant currently rejects eligible damage."
	);

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		State_MovementLocked,
		"State.MovementLocked",
		"The combatant cannot accept player-directed movement."
	);

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		State_Staggered,
		"State.Staggered",
		"The combatant is staggered and cannot start normal actions."
	);
}
