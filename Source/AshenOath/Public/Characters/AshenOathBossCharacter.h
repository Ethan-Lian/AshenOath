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
 * The Boss owns its ASC and AttributeSet. Both OwnerActor and AvatarActor
 * are this Character, and GAS initializes in BeginPlay without possession.
 */
UCLASS()
class ASHENOATH_API AAshenOathBossCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AAshenOathBossCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	
	const UAshenOathAttributeSet* GetAttributeSet() const;

protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void ApplyInitialAttributes();

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
};
