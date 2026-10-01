#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AshenOathCombatFeedbackComponent.generated.h"

class UCombatDamageComponent;
class UCombatDeathComponent;
class UParticleSystem;
class USoundBase;
struct FCombatDamageAttempt;
enum class ECombatDamageResult : uint8;

/** Project-owned audio and visual response to resolved combat events. */
UCLASS(ClassGroup = (AshenOath), meta = (BlueprintSpawnableComponent))
class ASHENOATH_API UAshenOathCombatFeedbackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAshenOathCombatFeedbackComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleDamageResolved(const FCombatDamageAttempt& Attempt, ECombatDamageResult Result);
	void HandleDeathStarted();

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Feedback|Hit")
	TObjectPtr<USoundBase> HitSound;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Feedback|Hit")
	TObjectPtr<UParticleSystem> HitEffect;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Feedback|Death")
	TObjectPtr<USoundBase> DeathSound;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Feedback|Death")
	TObjectPtr<UParticleSystem> DeathEffect;

	TWeakObjectPtr<UCombatDamageComponent> BoundDamageComponent;
	TWeakObjectPtr<UCombatDeathComponent> BoundDeathComponent;
	FDelegateHandle DamageResolvedHandle;
	FDelegateHandle DeathStartedHandle;
};
