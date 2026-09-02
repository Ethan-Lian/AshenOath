#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "AshenOathBossCharacter.generated.h"

class UAshenOathAttributeSet;
class UAbilitySystemComponent;
class UGameplayEffect;

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
	
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	
	const UAshenOathAttributeSet* GetAttributeSet() const; 

protected:
	virtual void BeginPlay() override;
	
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
private:
	void ApplyInitialAttributes();

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|AbilitySystem")
	TSubclassOf<UGameplayEffect> InitialAttributesEffect;

	// Guards against accidental duplicate initialization if the lifecycle is extended later.
	bool bInitialAttributesApplied = false;

	// Constructor-created subobjects are owned for the character's entire lifetime;
	// UPROPERTY/TObjectPtr makes that relationship visible to reflection and GC.
	UPROPERTY(
			VisibleAnywhere,
			BlueprintReadOnly,
			Category = "AshenOath|AbilitySystem",
			meta = (AllowPrivateAccess = "true")
		)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(
		VisibleAnywhere,
		Category = "AshenOath|AbilitySystem"
	)
	TObjectPtr<UAshenOathAttributeSet> AttributeSet;
};
