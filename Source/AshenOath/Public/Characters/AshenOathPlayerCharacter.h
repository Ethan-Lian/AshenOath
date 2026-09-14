#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Actions/CombatActionTypes.h"
#include "GameFramework/Character.h"
#include "GameplayAbilitySpecHandle.h"
#include "AshenOathPlayerCharacter.generated.h"

class UAshenOathAttributeSet;
class UAbilitySystemComponent;
class UGameplayEffect;
class UCameraComponent;
class USpringArmComponent;
class UCombatActionComponent;
class UCombatActionData;
class UCombatDamageComponent;
class UCombatDefenseComponent;
class UCombatMeleeComponent;
struct FGameplayTag;
class UGameplayAbility;
class UAshenOathLightAttackAbility;
class UAshenOathDodgeAbility;
class UAshenOathStaminaRecoveryComponent;


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

	// True means GAS accepted the activation request. It does not mean the
	// animation/cost/damage transaction has already completed.
	bool RequestLightAttack();

	// Selects the forward/backward AbilitySpec and freezes a world-space direction.
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

	// Retained only as the stage-D migration fallback; new combat requests use GAS.
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category = "AshenOath|Combat",meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCombatActionComponent> CombatActionComponent;

	// Owns weapon tracing, hit-window state, and per-window hit deduplication.
	// A light-attack Ability starts and ends one identity-bound detection session.
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category = "AshenOath|Combat",meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCombatMeleeComponent> CombatMeleeComponent;

	// Receives all incoming damage attempts and applies the shared
	// invulnerability contract before routing damage through GAS.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AshenOath|Combat",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCombatDamageComponent> CombatDamageComponent;

	// Owns dodge-window time/source identity and only its own loose tag count.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AshenOath|Combat",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCombatDefenseComponent> CombatDefenseComponent;

	// Recovery outlives any one action, so the character owns it independently.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AshenOath|Combat",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAshenOathStaminaRecoveryComponent> StaminaRecoveryComponent;

	// Data definition used when requesting the player's light attack.
	UPROPERTY(EditDefaultsOnly,Category = "AshenOath|Combat")
	TObjectPtr<UCombatActionData> LightAttackAction;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Combat|Abilities")
	TSubclassOf<UAshenOathLightAttackAbility> LightAttackAbilityClass;

	// Identifies the granted spec, not an individual execution.
	FGameplayAbilitySpecHandle LightAttackAbilitySpecHandle;

	// Forward dodge is also used for neutral and side input because the first
	// playable version reuses one forward flip for those directions.
	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Combat|Dodge")
	TObjectPtr<UCombatActionData> ForwardDodgeAction;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Combat|Dodge")
	TObjectPtr<UCombatActionData> BackwardDodgeAction;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Combat|Abilities")
	TSubclassOf<UAshenOathDodgeAbility> DodgeAbilityClass;

	FGameplayAbilitySpecHandle ForwardDodgeAbilitySpecHandle;
	FGameplayAbilitySpecHandle BackwardDodgeAbilitySpecHandle;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Combat|Stamina")
	TSubclassOf<UGameplayEffect> StaminaRecoveryEffect;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Combat|Stamina",meta = (ClampMin = "0.0", Units = "s"))
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

	void GrantConfiguredAbilities();
	void GrantAbilityIfNeeded(
		TSubclassOf<UGameplayAbility> AbilityClass,
		UCombatActionData* ActionData,
		FGameplayAbilitySpecHandle& InOutHandle
	);

	void CancelCombatAbilities();

	// Used only to keep the stage-D legacy fallback mutually exclusive.
	bool IsCombatAbilityActive() const;
};
