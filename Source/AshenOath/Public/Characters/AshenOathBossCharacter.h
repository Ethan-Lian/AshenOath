#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "GameplayAbilitySpecHandle.h"
#include "AshenOathBossCharacter.generated.h"

class UAshenOathAttributeSet;
class UAbilitySystemComponent;
class UGameplayEffect;
class UStateTreeComponent;
class UAshenOathMeleeActionData;
class UCombatDamageComponent;
class UCombatMeleeComponent;
class UAshenOathBossSingleSwingAbility;
class UCombatHitReactionComponent;
class UCombatDeathComponent;
struct FAbilityEndedData;

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

protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void ApplyInitialAttributes();
	bool GrantConfiguredAbilities();
	void CancelCombatAbilities();
	void HandleAbilityEnded(const FAbilityEndedData& EndedData);
	void HandleDeathStarted();

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

	FGameplayAbilitySpecHandle SingleSwingAbilitySpecHandle;
	FDelegateHandle AbilityEndedDelegateHandle;
	FDelegateHandle DeathStartedHandle;
	FSingleSwingEndedEvent SingleSwingEndedEvent;
};
