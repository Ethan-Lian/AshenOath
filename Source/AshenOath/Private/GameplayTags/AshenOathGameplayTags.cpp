#include "GameplayTags/AshenOathGameplayTags.h"

namespace AshenOathGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
	Ability_Action,
	"Ability.Action",
	"Classifies abilities that participate in combat-action mutual exclusion."
	);

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Ability_Action_LightAttack,
		"Ability.Action.LightAttack",
		"Identifies the player's light-attack ability."
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
