#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "CombatDeathComponent.generated.h"

class ACharacter;
class UAbilitySystemComponent;
class UAnimMontage;
struct FOnAttributeChangeData;

/** Owns one actor's terminal attribute transition and basic death presentation. */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class ASHENOATHCOMBAT_API UCombatDeathComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatDeathComponent();

	DECLARE_MULTICAST_DELEGATE(FDeathStartedEvent);

	// The game module injects its health attribute and terminal tag so this
	// reusable module does not depend on project-specific declarations.
	void ConfigureDeathContract(
		const FGameplayAttribute& HealthAttribute,
		const FGameplayTag& DeadStateTag
	);

	// Repeated initialization with the same ASC is a no-op. Passing a different
	// ASC replaces the previous observation only before death has started.
	bool Initialize(UAbilitySystemComponent* InAbilitySystemComponent);

	bool IsDeathStarted() const;

	FDeathStartedEvent& OnDeathStarted()
	{
		return DeathStartedEvent;
	}

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleHealthChanged(const FOnAttributeChangeData& ChangeData);
	bool TryStartDeath();
	void UnbindHealth();
	void PlayDeathMontage();

	UPROPERTY(EditAnywhere, Category = "Combat|Death")
	TObjectPtr<UAnimMontage> DeathMontage;

	FGameplayAttribute ObservedHealthAttribute;

	UPROPERTY(Transient)
	FGameplayTag TerminalStateTag;

	TWeakObjectPtr<ACharacter> CharacterOwner;
	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	FDelegateHandle HealthChangedHandle;
	FDeathStartedEvent DeathStartedEvent;

	bool bDeathStarted = false;
	bool bOwnsTerminalStateTag = false;
};
