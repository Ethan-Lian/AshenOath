#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Characters/AshenOathBossCharacter.h"
#include "AshenOathBossDecisionComponent.generated.h"

USTRUCT(BlueprintType)
struct FAshenOathBossAttackUtilityConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Utility", meta = (ClampMin = "0.0"))
	float BaseScore = 2.0f;

	UPROPERTY(EditAnywhere, Category = "Utility", meta = (ClampMin = "0.0", Units = "cm"))
	float PreferredDistance = 200.0f;

	UPROPERTY(EditAnywhere, Category = "Utility", meta = (ClampMin = "1.0", Units = "cm"))
	float DistanceFalloff = 300.0f;

	UPROPERTY(EditAnywhere, Category = "Utility", meta = (ClampMin = "0.0"))
	float DistanceWeight = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Utility", meta = (ClampMin = "0.0"))
	float FacingWeight = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Utility", meta = (ClampMin = "0.0"))
	float RecentUsePenalty = 0.75f;

	UPROPERTY(EditAnywhere, Category = "Utility", meta = (ClampMin = "0.0", Units = "s"))
	float CooldownSeconds = 2.0f;

	UPROPERTY(EditAnywhere, Category = "Utility", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PressureCost = 0.25f;
};

/** Pressure reaches 1 to request a breather; only Observe releases it. */
USTRUCT(BlueprintType)
struct FAshenOathBossPressureConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Pressure", meta = (ClampMin = "0.0", ClampMax = "0.99"))
	float ResumePressure = 0.25f;

	UPROPERTY(EditAnywhere, Category = "Pressure", meta = (ClampMin = "0.01"))
	float ReleasePerSecond = 0.35f;
};

struct FAshenOathBossScoredAttack
{
	EAshenOathBossAttackType AttackType = EAshenOathBossAttackType::SingleSwing;
	float BaseScore = 0.0f;
	float DistancePenalty = 0.0f;
	float FacingPenalty = 0.0f;
	float RecentPenalty = 0.0f;
	float TotalScore = 0.0f;
	float TieBreak = 0.0f;
};

struct FAshenOathBossCombatDecision
{
	EAshenOathBossCombatIntent Intent = EAshenOathBossCombatIntent::None;
	TWeakObjectPtr<AActor> TargetActor;
	float ApproachRange = 0.0f;
	float MeleeRange = 0.0f;
	float DashMinStartRange = 0.0f;
	float DashMaxStartRange = 0.0f;
	TArray<FAshenOathBossScoredAttack, TInlineAllocator<4>> RankedAttacks;
};

// Immutable inputs for one attack selection.
struct FAshenOathBossCombatSnapshot
{
	TWeakObjectPtr<AActor> TargetActor;
	float Distance2D = 0.0f;
	float FacingAngleDegrees = 0.0f;
	float WorldTimeSeconds = 0.0f;
	TArray<EAshenOathBossAttackType, TInlineAllocator<4>> AvailableAttacks;
	TArray<EAshenOathBossAttackType, TInlineAllocator<2>> RecentAcceptedAttacks;
	float LastAcceptedTimeSeconds[4] = {-1.0f, -1.0f, -1.0f, -1.0f};
};

/** Owns the Boss's first-phase selection state; the Character owns GAS execution. */
UCLASS()
class ASHENOATH_API UAshenOathBossDecisionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAshenOathBossDecisionComponent();

	void InitializeDecisionStream(int32 Seed);

	// Called exclusively by Observe Tick, after validating the current combat target.
	void ReleasePressureWhileObserving(float DeltaSeconds);
	bool ReconsiderCombatIntent();

	float GetPressure() const { return Pressure; }
	bool NeedsBreather() const { return bNeedsBreather; }
	float GetDecisionInterval() const { return 0.15f; }

	bool ChooseFirstPhaseCombatIntent(
		AActor* TargetActor,
		float MeleeRange,
		float DashMinStartRange,
		float DashMaxStartRange
	);

	EAshenOathBossCombatIntent GetPendingCombatIntent();

	void ClearPendingCombatDecision();

	bool IsCurrentCombatTarget(const AActor* TargetActor) const;

	bool TryConsumeApproachDecision(float& OutRange);

	FAshenOathBossAttackStartResult RequestSelectedCombatAttack();

private:
	bool CaptureFirstPhaseCombatSnapshot(
		AActor* TargetActor,
		FAshenOathBossCombatSnapshot& OutSnapshot
	) const;

	const FAshenOathBossAttackUtilityConfig& GetUtilityConfig(EAshenOathBossAttackType AttackType) const;

	FAshenOathBossScoredAttack ScoreAttack(
		const FAshenOathBossCombatSnapshot& Snapshot,
		EAshenOathBossAttackType AttackType
	) const;

	void BuildRankedAttacks(
		const FAshenOathBossCombatSnapshot& Snapshot,
		float MeleeRange,
		float DashMinStartRange,
		float DashMaxStartRange,
		TArray<FAshenOathBossScoredAttack, TInlineAllocator<4>>& OutRanked
	);

	void RecordAcceptedAttack(EAshenOathBossAttackType AttackType);

	FAshenOathBossCombatDecision PendingCombatDecision;
	FRandomStream CombatDecisionRandomStream;
	TArray<EAshenOathBossAttackType, TInlineAllocator<2>> RecentAcceptedAttacks;
	float LastAcceptedTimeSeconds[4] = {-1.0f, -1.0f, -1.0f, -1.0f};
	float Pressure = 0.0f;
	bool bNeedsBreather = false;
	float LastMeleeRange = 0.0f;
	float LastDashMinStartRange = 0.0f;
	float LastDashMaxStartRange = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|AI|Pressure")
	FAshenOathBossPressureConfig PressureConfig;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|AI|Utility")
	FAshenOathBossAttackUtilityConfig SingleSwingUtility;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|AI|Utility")
	FAshenOathBossAttackUtilityConfig ComboUtility;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|AI|Utility")
	FAshenOathBossAttackUtilityConfig ChargedSwingUtility;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|AI|Utility")
	FAshenOathBossAttackUtilityConfig DashSwingUtility;
};
