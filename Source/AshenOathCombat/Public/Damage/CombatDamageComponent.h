#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatDamageTypes.h"
#include "GameplayTagContainer.h"
#include "CombatDamageComponent.generated.h"

class UAbilitySystemComponent;

/**
 * Target-side entry point for combat damage.
 *
 * The component validates a damage attempt and routes its GameplayEffect from
 * the source ASC to the owning actor's ASC. Attribute values remain owned by GAS.
 */

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class ASHENOATHCOMBAT_API UCombatDamageComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatDamageComponent();

	// Validates and submits one incoming combat damage attempt.
	// Returns the result of validation and effect application.
	ECombatDamageResult ApplyDamageAttempt(const FCombatDamageAttempt& Attempt);

	// The game module supplies its native invulnerability tag so this reusable
	// component does not depend on project-specific tag declarations.
	void ConfigureInvulnerabilityTag(const FGameplayTag& Tag);
private:
	// Resolves an actor's ASC through IAbilitySystemInterface.
	static UAbilitySystemComponent* ResolveAbilitySystemComponent(AActor* Actor);

	UPROPERTY(EditAnywhere, Category = "Combat Damage")
	FGameplayTag InvulnerabilityTag;
};
