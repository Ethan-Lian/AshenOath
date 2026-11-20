#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "CombatDeathComponent.generated.h"

class ACharacter;
class UAbilitySystemComponent;
class UAnimMontage;
class UAnimInstance;
struct FOnAttributeChangeData;

/** Owns one actor's terminal attribute transition and basic death presentation. */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class ASHENOATHCOMBAT_API UCombatDeathComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatDeathComponent();

	DECLARE_MULTICAST_DELEGATE(FDeathStartedEvent);
	DECLARE_MULTICAST_DELEGATE(FDeathPresentationFinishedEvent);

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

	// Fires once when the owned Montage reaches its held final pose or is
	// interrupted; fires after DeathStarted immediately if playback fails.
	FDeathPresentationFinishedEvent& OnDeathPresentationFinished()
	{
		return DeathPresentationFinishedEvent;
	}

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleHealthChanged(const FOnAttributeChangeData& ChangeData);
	bool TryStartDeath();
	void UnbindHealth();
	void PlayDeathMontage();
	void HandleDeathMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void FinishDeathPresentation();
	void UnbindDeathMontage();

	UPROPERTY(EditAnywhere, Category = "Combat|Death")
	TObjectPtr<UAnimMontage> DeathMontage;

	FGameplayAttribute ObservedHealthAttribute;

	UPROPERTY(Transient)
	FGameplayTag TerminalStateTag;

	TWeakObjectPtr<ACharacter> CharacterOwner;
	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
	TWeakObjectPtr<UAnimInstance> DeathAnimInstance;
	int32 DeathMontageInstanceID = INDEX_NONE;

	FDelegateHandle HealthChangedHandle;
	FDeathStartedEvent DeathStartedEvent;
	FDeathPresentationFinishedEvent DeathPresentationFinishedEvent;

	bool bDeathStarted = false;
	bool bDeathPresentationFinished = false;
	bool bOwnsTerminalStateTag = false;
};
