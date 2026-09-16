#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "TimerManager.h"
#include "AshenOathStaminaRecoveryComponent.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;

/**
 * Owns stamina recovery state that spans multiple combat abilities.
 *
 * Successful stamina costs restart the delay. Rejected requests never call
 * this component, so they cannot postpone an already pending recovery.
 */
UCLASS(ClassGroup = (AshenOath), meta = (BlueprintSpawnableComponent))
class ASHENOATH_API UAshenOathStaminaRecoveryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAshenOathStaminaRecoveryComponent();

	// Stops any current recovery before replacing the effect and delay.
	void Configure(TSubclassOf<UGameplayEffect> RecoveryEffect, float DelaySeconds);
	void NotifyStaminaCostCommitted();
	void StopRecovery();
	bool HasPendingOrActiveRecovery() const;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UAbilitySystemComponent* ResolveAbilitySystemComponent() const;
	void RestartRecovery();
	void StartRecovery();

	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> ConfiguredRecoveryEffect;

	float RecoveryDelay = 0.0f;
	FTimerHandle RecoveryTimer;
	FActiveGameplayEffectHandle ActiveRecoveryEffect;
	uint32 RecoveryGeneration = 0;
};
