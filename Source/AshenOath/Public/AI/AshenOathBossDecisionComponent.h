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
	TArray<FAshenOathBossScoredAttack, TInlineAllocator<4>> RankedAttacks;
};

// One observation taken at Decide, not continuously refreshed each Tick.
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

	bool ChooseFirstPhaseCombatIntent(
		AActor* TargetActor,
		float MeleeRange,
		float DashMinStartRange
	);

	EAshenOathBossCombatIntent GetPendingCombatIntent();

	void ClearPendingCombatDecision();

	bool IsCurrentCombatTarget(const AActor* TargetActor) const;

	bool TryConsumeApproachDecision(float& OutRange);

	FAshenOathBossAttackStartResult RequestSelectedCombatAttack();

#if WITH_DEV_AUTOMATION_TESTS
	const FAshenOathBossCombatDecision& GetPendingDecisionForTesting() const
	{
		return PendingCombatDecision;
	}
#endif

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
		TArray<FAshenOathBossScoredAttack, TInlineAllocator<4>>& OutRanked
	);

	void RecordAcceptedAttack(EAshenOathBossAttackType AttackType);

	FAshenOathBossCombatDecision PendingCombatDecision;
	FRandomStream CombatDecisionRandomStream;
	TArray<EAshenOathBossAttackType, TInlineAllocator<2>> RecentAcceptedAttacks;
	float LastAcceptedTimeSeconds[4] = {-1.0f, -1.0f, -1.0f, -1.0f};

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|AI|Utility")
	FAshenOathBossAttackUtilityConfig SingleSwingUtility;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|AI|Utility")
	FAshenOathBossAttackUtilityConfig ComboUtility;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|AI|Utility")
	FAshenOathBossAttackUtilityConfig ChargedSwingUtility;

	UPROPERTY(EditDefaultsOnly, Category = "AshenOath|AI|Utility")
	FAshenOathBossAttackUtilityConfig DashSwingUtility;
};
