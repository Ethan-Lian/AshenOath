#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "GameplayAbilitySpecHandle.h"
#include "AshenOathBossCharacter.generated.h"

class UAshenOathAttributeSet;
class UAbilitySystemComponent;
class UGameplayEffect;
class UGameplayAbility;
class UStateTreeComponent;
class UAshenOathMeleeActionData;
class UAshenOathBossComboActionData;
class UAshenOathBossChargedSwingActionData;
class UAshenOathBossDashSwingActionData;
class UCombatDamageComponent;
class UCombatMeleeComponent;
class UAshenOathBossSingleSwingAbility;
class UAshenOathBossComboAbility;
class UAshenOathBossChargedSwingAbility;
class UAshenOathBossDashSwingAbility;
class UCombatHitReactionComponent;
class UCombatDeathComponent;
struct FAbilityEndedData;

UENUM()
enum class EAshenOathBossAttackType : uint8
{
	SingleSwing,
	Combo,
	ChargedSwing,
	DashSwing
};

enum class EAshenOathBossAttackStartState : uint8
{
	Rejected,
	Running,
	Succeeded,
	Failed
};

struct FAshenOathBossAttackRequestHandle
{
	uint64 Value = 0;

	bool IsValid() const
	{
		return Value != 0;
	}

	bool operator==(const FAshenOathBossAttackRequestHandle& Other) const
	{
		return Value == Other.Value;
	}
};

struct FAshenOathBossAttackStartResult
{
	EAshenOathBossAttackStartState State = EAshenOathBossAttackStartState::Rejected;
	FAshenOathBossAttackRequestHandle RequestHandle;
};


/**
 * AI-side GAS host.
 *
 * The Boss owns its ASC and AttributeSet. Both OwnerActor and AvatarActor
 * are this Character, and GAS initializes in BeginPlay without possession.
 */
UCLASS()
class ASHENOATH_API AAshenOathBossCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AAshenOathBossCharacter();

	DECLARE_MULTICAST_DELEGATE_OneParam(FSingleSwingEndedEvent, bool);

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	const UAshenOathAttributeSet* GetAttributeSet() const;

	// True means GAS accepted the configured single-swing activation request.
	bool RequestSingleSwing();
	bool IsSingleSwingActive() const;
	void CancelSingleSwing();

	FSingleSwingEndedEvent& OnSingleSwingEnded()
	{
		return SingleSwingEndedEvent;
	}

#if WITH_DEV_AUTOMATION_TESTS
	// Supplies transient test configuration without exposing mutable runtime setup.
	void GrantSingleSwingForTesting(UAshenOathMeleeActionData* ActionData);
#endif

	DECLARE_MULTICAST_DELEGATE_TwoParams(
	FAshenOathBossAttackEndedEvent,
	FAshenOathBossAttackRequestHandle,
	bool
	);

	// A running result owns a unique request handle. A synchronous Ability end is
	// returned as Succeeded/Failed; only later ends use OnBossAttackEnded().
	FAshenOathBossAttackStartResult RequestBossAttack(EAshenOathBossAttackType AttackType);

	// Rotates available melee attacks and avoids selecting two dashes in a row.
	// Only accepted requests advance the selection history.
	FAshenOathBossAttackStartResult RequestFirstPhaseAttack(
		float TargetDistance,
		float DashMinRange
	);

	// Cancels only the currently owned request; expired handles have no effect.
	bool CancelBossAttack(FAshenOathBossAttackRequestHandle Request);

	FAshenOathBossAttackEndedEvent& OnBossAttackEnded()
	{
		return BossAttackEndedEvent;
	}
protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void ApplyInitialAttributes();

	bool GrantConfiguredAbilities();

	bool GrantAbilityIfNeeded(
		TSubclassOf<UGameplayAbility> AbilityClass,
		UAshenOathMeleeActionData* ActionData,
		FGameplayAbilitySpecHandle& InOutHandle
	);

	void CancelCombatAbilities();

	void HandleAbilityEnded(const FAbilityEndedData& EndedData);

	void HandleDeathStarted();

	FGameplayAbilitySpecHandle ResolveBossAttackSpec(EAshenOathBossAttackType AttackType) const;

	void FinishBossAttackRequest(bool bWasCancelled);

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|AbilitySystem")
	TSubclassOf<UGameplayEffect> InitialAttributesEffect;

	bool bInitialAttributesApplied = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AshenOath|AbilitySystem",meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, Category = "AshenOath|AbilitySystem")
	TObjectPtr<UAshenOathAttributeSet> AttributeSet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AshenOath|AI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStateTreeComponent> StateTreeComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AshenOath|Combat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCombatDamageComponent> CombatDamageComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AshenOath|Combat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCombatMeleeComponent> CombatMeleeComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AshenOath|Combat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCombatHitReactionComponent> HitReactionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AshenOath|Combat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCombatDeathComponent> DeathComponent;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Combat|Boss")
	TObjectPtr<UAshenOathMeleeActionData> SingleSwingAction;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Combat|Boss")
	TSubclassOf<UAshenOathBossSingleSwingAbility> SingleSwingAbilityClass;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Combat|Boss")
	TObjectPtr<UAshenOathBossComboActionData> ComboAction;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Combat|Boss")
	TSubclassOf<UAshenOathBossComboAbility> ComboAbilityClass;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Combat|Boss")
	TObjectPtr<UAshenOathBossChargedSwingActionData> ChargedSwingAction;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Combat|Boss")
	TSubclassOf<UAshenOathBossChargedSwingAbility> ChargedSwingAbilityClass;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Combat|Boss")
	TObjectPtr<UAshenOathBossDashSwingActionData> DashSwingAction;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Combat|Boss")
	TSubclassOf<UAshenOathBossDashSwingAbility> DashSwingAbilityClass;

	FGameplayAbilitySpecHandle SingleSwingAbilitySpecHandle;
	FGameplayAbilitySpecHandle ComboAbilitySpecHandle;
	FGameplayAbilitySpecHandle ChargedSwingAbilitySpecHandle;
	FGameplayAbilitySpecHandle DashSwingAbilitySpecHandle;

	FAshenOathBossAttackRequestHandle ActiveBossAttackRequest;
	FGameplayAbilitySpecHandle ActiveBossAttackSpec;

	FDelegateHandle AbilityEndedDelegateHandle;
	FDelegateHandle DeathStartedHandle;

	FSingleSwingEndedEvent SingleSwingEndedEvent;
	FAshenOathBossAttackEndedEvent BossAttackEndedEvent;

	uint64 NextBossAttackRequestValue = 1;
	int32 NextNearAttackIndex = 0;
	bool bLastAttackWasDash = false;
	bool bStartingBossAttack = false;
	bool bSynchronousBossAttackEnded = false;
	bool bSynchronousBossAttackWasCancelled = false;
	bool bCancelBossAttackAfterStart = false;
};
