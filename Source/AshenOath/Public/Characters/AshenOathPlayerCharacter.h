#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Actions/CombatActionTypes.h"
#include "GameFramework/Character.h"
#include "AshenOathPlayerCharacter.generated.h"

class UAshenOathAttributeSet;
class UAbilitySystemComponent;
class UGameplayEffect;
class UCameraComponent;
class USpringArmComponent;
class UCombatActionComponent;
class UCombatActionData;
class UCombatDamageComponent;
class UCombatMeleeComponent;
struct FGameplayTag;


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

	// Input flow: Controller -> Character -> CombatActionComponent -> animation.
	ECombatActionStartResult RequestLightAttack();

	// Selects the available forward/backward dodge presentation from current input.
	// Movement and invulnerability are added by the following combat-action stages.
	ECombatActionStartResult RequestDodge(const FVector2D& MovementIntent);
protected:
	virtual void BeginPlay() override;
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

	// Owns weapon tracing, hit-window state, and per-window hit deduplication.
	// CombatActionComponent starts and ends its state with each action execution.
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category = "AshenOath|Combat",meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCombatMeleeComponent> CombatMeleeComponent;

	// Receives all incoming damage attempts and applies the shared
	// invulnerability contract before routing damage through GAS.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AshenOath|Combat",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCombatDamageComponent> CombatDamageComponent;

	// Data definition used when requesting the player's light attack.
	UPROPERTY(EditDefaultsOnly,Category = "AshenOath|Combat")
	TObjectPtr<UCombatActionData> LightAttackAction;

	// Forward dodge is also used for neutral and side input because the first
	// playable version reuses one forward flip for those directions.
	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Combat|Dodge")
	TObjectPtr<UCombatActionData> ForwardDodgeAction;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Combat|Dodge")
	TObjectPtr<UCombatActionData> BackwardDodgeAction;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Combat|Stamina")
	TSubclassOf<UGameplayEffect> StaminaRecoveryEffect;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Combat|Stamina",
		meta = (ClampMin = "0.0", Units = "s"))
	float StaminaRecoveryDelay = 1.0f;
	
	// GameplayEffect class used to initialize the Character's starting attributes.
	UPROPERTY(EditDefaultsOnly,Category = "AshenOath|AbilitySystem")
	TSubclassOf<UGameplayEffect> InitialAttributesEffect;
	
	UPROPERTY(VisibleAnywhere, Category = "AshenOath|Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = "AshenOath|Camera")
	TObjectPtr<UCameraComponent> FollowCamera;
	
	// PossessedBy may run again after repossession; initial stats are applied only once.
	bool bInitialAttributesApplied = false;
	FDelegateHandle DeadStateChangedHandle;

	// Applies the startup GameplayEffect that establishes initial attribute values.
	void ApplyInitialAttributes();

	// Shared character-level state gate before delegating to the generic action component.
	ECombatActionStartResult TryStartCombatAction(const UCombatActionData* ActionData,
	                                             const FVector& MovementDirection = FVector::ZeroVector);

	void HandleDeadStateChanged(const FGameplayTag Tag, int32 NewCount);
};
