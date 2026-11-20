#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Feedback/AshenOathVocalTypes.h"
#include "AshenOathCombatFeedbackComponent.generated.h"

class UCombatDamageComponent;
class UAudioComponent;
class UCombatDeathComponent;
class UParticleSystem;
class USoundBase;
class USoundConcurrency;
class USkeletalMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UParticleSystemComponent;
class UAshenOathHeavyAttackData;
class UPointLightComponent;
struct FAshenOathChargeState;
struct FCombatDamageAttempt;
struct FHitResult;
enum class ECombatDamageResult : uint8;

/** Project-owned audio and visual response to resolved combat events. */
UCLASS(ClassGroup = (AshenOath), meta = (BlueprintSpawnableComponent))
class ASHENOATH_API UAshenOathCombatFeedbackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAshenOathCombatFeedbackComponent();

	// Ordinary efforts share one budget; a heavy combo finisher has its own slot.
	uint32 BeginVocalAction();
	void EndVocalAction(uint32 ActionId, bool bWasCancelled);
	bool RequestActionVocal(uint32 ActionId, EAshenOathVocalType Type, USoundBase* Sound);
	void StopCombatVocals();
	float GetDeathFinishDuration() const;
	void ConfigureDeathAnimationDrivenBone(FName BoneName);

	uint32 BeginChargeFeedback(const UAshenOathHeavyAttackData* Data);
	void UpdateChargeFeedback(uint32 ChargeId, const FAshenOathChargeState& State);
	void ReleaseChargeFeedback(uint32 ChargeId, float ChargeAlpha);
	void EndChargeFeedback(uint32 ChargeId, bool bWasCancelled);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	enum class EVocalPriority : uint8
	{
		Light,
		Heavy,
		Special,
		Pain,
		Death
	};

	bool PlayVocal(USoundBase* Sound, float Volume, float Pitch, EVocalPriority Priority, uint32 ActionId = 0);
	void StopActiveVocal();
	void HandleDamageResolved(const FCombatDamageAttempt& Attempt, ECombatDamageResult Result);
	void HandleDeathStarted();
	void HandleDeathAnimationFinished();
	void StartDeathRagdoll();
	void PlayDeathGroundImpact(const FVector& Location, const FVector& Normal);

	void UpdateDeathGroundContact();

	void UpdateDeathMaterials(float Progress);
	void UpdateDeathCamera(float DeltaTime);
	void StopDeathEffects();
	void StopChargeLoop();
	void PlayChargeCue(USoundBase* Sound, UParticleSystem* Effect, float Scale);

	TWeakObjectPtr<const UAshenOathHeavyAttackData> ActiveChargeData;
	TWeakObjectPtr<UAudioComponent> ChargeLoopAudio;
	TWeakObjectPtr<UParticleSystemComponent> ChargeLoopParticle;
	TWeakObjectPtr<UPointLightComponent> ChargeLight;
	TArray<TWeakObjectPtr<UAudioComponent>> ChargeCueAudio;
	TArray<TWeakObjectPtr<UParticleSystemComponent>> ChargeCueParticles;
	uint32 NextChargeId = 0;
	uint32 ActiveChargeId = 0;
	bool bChargeReadyPlayed = false;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Feedback|Death")
	TObjectPtr<UParticleSystem> DeathGroundEffect;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Feedback|Death")
	TObjectPtr<USoundBase> DeathGroundSound;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Feedback|Death", meta = (ClampMin = "0"))
	float DeathGroundVolume = 0.8f;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Feedback|Death", meta = (ClampMin = "0.1"))
	float DeathGroundScale = 1.0f;

	// The final pose stays visible before the material erosion starts.
	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Feedback|Death", meta = (ClampMin = "0", Units = "s"))
	float DeathPoseHoldSeconds = 0.45f;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Feedback|Death", meta = (ClampMin = "0", Units = "s"))
	float DeathFadeSeconds = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Feedback|Death")
	FName DeathDissolveParameter = NAME_None;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Feedback|Death")
	TArray<FName> DeathEmissiveParameters;

	// Optional copies of the character materials with the authored death mask.
	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Feedback|Death")
	TArray<TObjectPtr<UMaterialInterface>> DeathDissolveMaterials;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> DeathMaterials;
	TArray<TMap<FName, float>> DeathInitialEmissive;
	TArray<TWeakObjectPtr<UParticleSystemComponent>> DeathParticles;
	FDelegateHandle DeathFinishedHandle;
	double DeathAnimationFinishedTime = -1.0;
	TWeakObjectPtr<USkeletalMeshComponent> DeathRagdollMesh;
	FName DeathAnimationDrivenBone = NAME_None;
	bool bDeathGroundPlayed = false;
	bool bDeathReleasePlayed = false;

	// Contact layer plays for every applied hit, including the killing blow.
	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Feedback|Hit")
	TObjectPtr<USoundBase> ImpactSound;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Feedback|Hit", meta = (ClampMin = "0"))
	float ImpactVolume = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Feedback|Hit")
	TObjectPtr<USoundBase> HitSound;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Feedback|Hit")
	TObjectPtr<UParticleSystem> HitEffect;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Feedback|Death")
	TObjectPtr<USoundBase> DeathSound;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Feedback|Death")
	TObjectPtr<UParticleSystem> DeathEffect;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Feedback|Death", meta = (ClampMin = "0.01"))
	float DeathEffectScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Feedback|Vocals")
	TMap<EAshenOathVocalType, FAshenOathVocalRule> VocalRules;

	// Shared across attack types and dodge; ending an action does not reset it.
	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Feedback|Vocals", meta = (ClampMin = "0", Units = "s"))
	float EffortCooldown = 2.5f;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Feedback|Vocals", meta = (ClampMin = "0", Units = "s"))
	float PainCooldown = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Feedback|Vocals", meta = (ClampMin = "0"))
	float PainVolume = 0.65f;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Feedback|Vocals", meta = (ClampMin = "0"))
	float DeathVolume = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|Feedback|Vocals")
	TObjectPtr<USoundConcurrency> VocalConcurrency;

	TWeakObjectPtr<UAudioComponent> ActiveVocal;
	EVocalPriority ActiveVocalPriority = EVocalPriority::Light;
	uint32 NextVocalActionId = 0;
	uint32 ActiveVocalActionId = 0;
	uint32 PlayingVocalActionId = 0;
	bool bActionVocalPlayed = false;
	bool bFinisherVocalPlayed = false;
	bool bCombatEnded = false;
	bool bDeathVocalRequested = false;
	double NextEffortTime = 0.0;
	double NextPainTime = 0.0;

	TWeakObjectPtr<UCombatDamageComponent> BoundDamageComponent;
	TWeakObjectPtr<UCombatDeathComponent> BoundDeathComponent;
	FDelegateHandle DamageResolvedHandle;
	FDelegateHandle DeathStartedHandle;
};
