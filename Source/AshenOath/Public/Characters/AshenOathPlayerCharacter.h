#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "GameplayAbilitySpecHandle.h"
#include "AshenOathPlayerCharacter.generated.h"

class UAshenOathAttributeSet;
class UAbilitySystemComponent;
class UGameplayEffect;
class UCameraComponent;
class USpringArmComponent;
class UAshenOathActionData;
class UAshenOathDodgeActionData;
class UCombatDamageComponent;
class UCombatDefenseComponent;
class UCombatMeleeComponent;
struct FGameplayTag;
class UGameplayAbility;
class UAshenOathComboAttackAbility;
class UAshenOathComboAttackData;
class UAshenOathHeavyAttackAbility;
class UAshenOathHeavyAttackData;
class UAshenOathHealAbility;
class UAshenOathHealActionData;
class UAshenOathDodgeAbility;
class UAshenOathStaminaRecoveryComponent;
class UCombatHitReactionComponent;
class UCombatDeathComponent;
class UCombatTargetingComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnAshenOathHealUsesChanged, int32);

/**
 * Player-side GAS host, movement and combat actions.
 */
UCLASS()
class ASHENOATH_API AAshenOathPlayerCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AAshenOathPlayerCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	const UAshenOathAttributeSet* GetAttributeSet() const;

	// Converts camera-space input into world-space directions for CharacterMovement.
	void RequestMove(const FVector2D& MovementIntent, float ReferenceYaw);

	// True means GAS accepted the activation request. It does not mean the
	// animation/cost/damage transaction has already completed.
	bool RequestComboAttack();

	// Press starts the tap-or-hold decision; release resolves the active Heavy
	// Ability without exposing its internal phase to the Controller.
	bool RequestHeavyAttackPressed();
	bool RequestHeavyAttackReleased();

	// Requests one interruptible Cast. The accepted request does not spend a use.
	bool RequestHeal();
	bool CanStartHeal() const;
	int32 GetRemainingHealUses() const { return RemainingHealUses; }
	FOnAshenOathHealUsesChanged& OnHealUsesChanged() { return HealUsesChangedEvent; }

	// Selects the forward/backward AbilitySpec and freezes a world-space direction.
	// True means GAS accepted the activation request; completion remains asynchronous.
	bool RequestDodge(const FVector2D& MovementIntent);

	// Toggles the currently registered Boss as the lock target; true means the
	// target changed successfully.
	bool RequestToggleLockOn();

	// Clears the current lock target, if any.
	void RequestClearLockOn();

	// Returns whether a valid lock target is currently owned.
	bool IsLockedOn() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;

private:
	friend class UAshenOathHealAbility;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AshenOath|AbilitySystem",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, Category = "AshenOath|AbilitySystem")
	TObjectPtr<UAshenOathAttributeSet> AttributeSet;

	// Owns weapon tracing, hit-window state, and per-window hit deduplication.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AshenOath|Combat",
		meta = (AllowPrivateAccess = "true"))
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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AshenOath|Combat",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCombatHitReactionComponent> HitReactionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AshenOath|Combat",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCombatDeathComponent> DeathComponent;

	// Immutable configuration supplied to the granted combo-attack AbilitySpec.
	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Combat")
	TObjectPtr<UAshenOathComboAttackData> ComboAttackAction;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Combat|Abilities")
	TSubclassOf<UAshenOathComboAttackAbility> ComboAttackAbilityClass;

	// Identifies the granted spec, not an individual execution.
	FGameplayAbilitySpecHandle ComboAttackAbilitySpecHandle;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Combat|Heavy Attack")
	TObjectPtr<UAshenOathHeavyAttackData> HeavyAttackAction;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Combat|Abilities")
	TSubclassOf<UAshenOathHeavyAttackAbility> HeavyAttackAbilityClass;

	FGameplayAbilitySpecHandle HeavyAttackAbilitySpecHandle;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Combat|Heal")
	TObjectPtr<UAshenOathHealActionData> HealAction;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Combat|Abilities")
	TSubclassOf<UAshenOathHealAbility> HealAbilityClass;

	FGameplayAbilitySpecHandle HealAbilitySpecHandle;
	int32 RemainingHealUses = 3;
	bool bHealUseReserved = false;
	FOnAshenOathHealUsesChanged HealUsesChangedEvent;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Combat|Dodge")
	TObjectPtr<UAshenOathDodgeActionData> ForwardDodgeAction;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Combat|Dodge")
	TObjectPtr<UAshenOathDodgeActionData> BackwardDodgeAction;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Combat|Abilities")
	TSubclassOf<UAshenOathDodgeAbility> DodgeAbilityClass;

	FGameplayAbilitySpecHandle ForwardDodgeAbilitySpecHandle;
	FGameplayAbilitySpecHandle BackwardDodgeAbilitySpecHandle;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Combat|Stamina")
	TSubclassOf<UGameplayEffect> StaminaRecoveryEffect;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Combat|Stamina",
		meta = (ClampMin = "0.0", Units = "s"))
	float StaminaRecoveryDelay = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|AbilitySystem")
	TSubclassOf<UGameplayEffect> InitialAttributesEffect;

	UPROPERTY(VisibleAnywhere, Category = "AshenOath|Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = "AshenOath|Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	// Owns target identity and validity; movement/camera consume its state.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AshenOath|Lock On",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCombatTargetingComponent> CombatTargetingComponent;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Lock On",
		meta = (ClampMin = "0.0"))
	float LockOnViewInterpSpeed = 8.0f;

	// PossessedBy may run again after repossession; initial stats are applied only once.
	bool bInitialAttributesApplied = false;
	FDelegateHandle DeathStartedHandle;
	FDelegateHandle MovementLockedStateChangedHandle;
	FDelegateHandle StaggeredStateChangedHandle;
	FDelegateHandle LockTargetChangedHandle;

	// Applies the startup GameplayEffect that establishes initial attribute values.
	void ApplyInitialAttributes();

	void HandleDeathStarted();
	void HandleMovementLockedStateChanged(const FGameplayTag Tag, int32 NewCount);
	void HandleLockTargetChanged(AActor* PreviousTarget, AActor* NewTarget);
	void UpdateLockedView(float DeltaSeconds);
	void RefreshFacingMode();
	FVector CalculateDodgeDirection(
		const FVector2D& DodgeIntent,
		const UAshenOathDodgeActionData* DodgeAction) const;

	void GrantConfiguredAbilities();
	void GrantAbilityIfNeeded(
		TSubclassOf<UGameplayAbility> AbilityClass,
		UAshenOathActionData* ActionData,
		FGameplayAbilitySpecHandle& InOutHandle
	);

	void CancelCombatAbilities();
	bool TryReserveHealUse();
	void ResolveHealUseReservation(bool bEffectApplied);
};
