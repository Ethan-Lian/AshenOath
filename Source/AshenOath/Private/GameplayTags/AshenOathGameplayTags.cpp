#include "GameplayTags/AshenOathGameplayTags.h"

namespace AshenOathGameplayTags
{
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
		State_Staggered,
		"State.Staggered",
		"The combatant is staggered and cannot start normal actions."
	);
}
