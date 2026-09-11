#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "AshenOathPlayerCharacter.generated.h"

class UAshenOathAttributeSet;
class UAbilitySystemComponent;
class UGameplayEffect;
class UCameraComponent;
class USpringArmComponent;
class UCombatActionComponent;
class UCombatActionData;


/**
 * Player-side GAS host, movement and combat actions.
 *
 * The character owns both the AbilitySystemComponent (ASC) and AttributeSet, so
 * the character is both GAS OwnerActor and AvatarActor. This keeps ownership
 * simple while attributes and abilities live only as long as the pawn.
 */
UCLASS()
class ASHENOATH_API AAshenOathPlayerCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AAshenOathPlayerCharacter();

	// Exposes this Character's ASC through UE's standard AbilitySystem interface.
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	
	// Read-only access to the Character's GAS-backed gameplay attributes.
	const UAshenOathAttributeSet* GetAttributeSet() const;
	
	// Convert 2D movement input from camera space into world-space movement directions, then pass it to CharacterMovement.
	void RequestMove(const FVector2D& MovementIntent, float ReferenceYaw);

	// Input flow: Controller -> Character -> CombatActionComponent -> animation
	void RequestLightAttack();
protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	virtual void PossessedBy(AController* NewController) override;

private:
	// Constructor-created subobjects share the character's lifetime. UPROPERTY/TObjectPtr
	// keeps them visible to reflection and tracked by Unreal's object/GC system.

	// Core GAS component owned by this Character for its lifetime.
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category = "AshenOath|AbilitySystem",meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	// Stores gameplay attributes such as Health
	UPROPERTY(VisibleAnywhere,Category = "AshenOath|AbilitySystem")
	TObjectPtr<UAshenOathAttributeSet> AttributeSet;

	// Executes character combat actions and owns their runtime action state.
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category = "AshenOath|Combat",meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCombatActionComponent> CombatActionComponent;

	// Data definition used when requesting the player's light attack.
	UPROPERTY(EditDefaultsOnly,Category = "AshenOath|Combat")
	TObjectPtr<UCombatActionData> LightAttackAction;
	
	// GameplayEffect class used to initialize the Character's starting attributes.
	UPROPERTY(EditDefaultsOnly,Category = "AshenOath|AbilitySystem")
	TSubclassOf<UGameplayEffect> InitialAttributesEffect;
	
	UPROPERTY(VisibleAnywhere, Category = "AshenOath|Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = "AshenOath|Camera")
	TObjectPtr<UCameraComponent> FollowCamera;
	
	// PossessedBy may run again after repossession; initial stats are applied only once.
	bool bInitialAttributesApplied = false;

	// Applies the startup GameplayEffect that establishes initial attribute values.
	void ApplyInitialAttributes();
};
