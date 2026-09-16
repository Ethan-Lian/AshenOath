#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "AshenOathAttributeSet.generated.h"

UCLASS()
class ASHENOATH_API UAshenOathAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	/*
	 * Attribute constraint pipeline:
	 * 1. PreAttributeChange clamps the incoming value before GAS commits the change.
	 * 2. PostAttributeChange enforces the cross-attribute constraint Current <= Max
	 *    when MaxHealth or MaxStamina changes.
	 * 3. PostGameplayEffectExecute normalizes the committed value after a
	 *    GameplayEffect execution modifies an attribute's base value.
	 */

	virtual void PreAttributeChange(
		const FGameplayAttribute& Attribute,
		float& NewValue
	) override;

	virtual void PostAttributeChange(
		const FGameplayAttribute& Attribute,
		float OldValue,
		float NewValue
	) override;

	virtual void PostGameplayEffectExecute(
		const FGameplayEffectModCallbackData& Data
	) override;
	
	ATTRIBUTE_ACCESSORS_BASIC(UAshenOathAttributeSet, Health);
	ATTRIBUTE_ACCESSORS_BASIC(UAshenOathAttributeSet, MaxHealth);
	ATTRIBUTE_ACCESSORS_BASIC(UAshenOathAttributeSet, Stamina);
	ATTRIBUTE_ACCESSORS_BASIC(UAshenOathAttributeSet, MaxStamina);

protected:

	UPROPERTY(BlueprintReadOnly, Category = "AshenOath|Attributes|Health")
	FGameplayAttributeData Health;

	UPROPERTY(BlueprintReadOnly, Category = "AshenOath|Attributes|Health")
	FGameplayAttributeData MaxHealth;

	UPROPERTY(BlueprintReadOnly, Category = "AshenOath|Attributes|Stamina")
	FGameplayAttributeData Stamina;

	UPROPERTY(BlueprintReadOnly, Category = "AshenOath|Attributes|Stamina")
	FGameplayAttributeData MaxStamina;
};


