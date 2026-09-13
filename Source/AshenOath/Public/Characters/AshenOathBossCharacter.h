#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "AshenOathBossCharacter.generated.h"

class UAshenOathAttributeSet;
class UAbilitySystemComponent;
class UGameplayEffect;
class UStateTreeComponent;
class UCombatDamageComponent;

/**
 * AI-side GAS host.
 *
 * Bosses own their ASC and AttributeSet directly, so OwnerActor and AvatarActor
 * are both this character. Unlike a player pawn, a boss needs no possession event;
 * its GAS state becomes usable when the actor enters play.
 */
UCLASS()
class ASHENOATH_API AAshenOathBossCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AAshenOathBossCharacter();

	// Exposes the boss ASC through UE's standard AbilitySystem interface.
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	
	// Read-only access to the boss gameplay attributes.
	const UAshenOathAttributeSet* GetAttributeSet() const;

protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
  	// Applies the startup GameplayEffect that initializes the boss attributes.
	void ApplyInitialAttributes();

	// GameplayEffect used to establish initial Attribute.
	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|AbilitySystem")
	TSubclassOf<UGameplayEffect> InitialAttributesEffect;

	// Guards against accidental duplicate initialization if the lifecycle is extended later.
	bool bInitialAttributesApplied = false;

	// UPROPERTY/TObjectPtr makes that relationship visible to reflection and GC.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AshenOath|AbilitySystem",meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, Category = "AshenOath|AbilitySystem")
	TObjectPtr<UAshenOathAttributeSet> AttributeSet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AshenOath|AI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStateTreeComponent> StateTreeComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AshenOath|Combat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCombatDamageComponent> CombatDamageComponent;
};
