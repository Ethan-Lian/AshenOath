#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Damage/CombatDamageTypes.h"
#include "GameplayTagContainer.h"
#include "CombatHitReactionComponent.generated.h"

class ACharacter;
class UAbilitySystemComponent;
class UAnimMontage;
class UCombatDamageComponent;

/** Owns one target's short stagger state and basic Montage hit response. */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class ASHENOATHCOMBAT_API UCombatHitReactionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatHitReactionComponent();

	// The game module supplies its native tags so this reusable component does
	// not depend on project-specific tag declarations.
	void ConfigureGameplayTags(
		const FGameplayTag& ReactionTag,
		const FGameplayTag& TerminalStateTag,
		const FGameplayTag& InterruptibleAbilityTag
	);

	// Releases reaction-owned state before a terminal animation takes over.
	void ResetReaction();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleDamageResolved(
		const FCombatDamageAttempt& Attempt,
		ECombatDamageResult Result
	);
	void EndHitReaction();

	UPROPERTY(EditAnywhere, Category = "Combat|Hit Reaction")
	TObjectPtr<UAnimMontage> HitReactionMontage;

	UPROPERTY(EditAnywhere, Category = "Combat|Hit Reaction",
		meta = (ClampMin = "0.01", Units = "s"))
	float StaggerDuration = 0.35f;

	UPROPERTY(Transient)
	FGameplayTag ReactionStateTag;

	UPROPERTY(Transient)
	FGameplayTag TerminalStateTag;

	UPROPERTY(Transient)
	FGameplayTag InterruptibleAbilityTag;

	TWeakObjectPtr<ACharacter> CharacterOwner;
	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
	TWeakObjectPtr<UCombatDamageComponent> BoundDamageComponent;
	FDelegateHandle DamageResolvedDelegateHandle;
	FTimerHandle StaggerTimerHandle;
	bool bOwnsReactionTag = false;
};
