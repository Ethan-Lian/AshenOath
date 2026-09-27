#include "AI/AshenOathBossDecisionComponent.h"

#include "AbilitySystem/Data/AshenOathBossDashSwingActionData.h"
#include "Death/CombatDeathComponent.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogAshenOathBossUtility, Log, All);

namespace
{
	constexpr int32 BossAttackCount = 4;
}

UAshenOathBossDecisionComponent::UAshenOathBossDecisionComponent()
{
	SingleSwingUtility.PreferredDistance = 170.0f;
	SingleSwingUtility.CooldownSeconds = 0.75f;

	ComboUtility.PreferredDistance = 210.0f;
	ComboUtility.FacingWeight = 0.75f;
	ComboUtility.RecentUsePenalty = 1.0f;
	ComboUtility.CooldownSeconds = 2.5f;

	ChargedSwingUtility.PreferredDistance = 260.0f;
	ChargedSwingUtility.FacingWeight = 0.9f;
	ChargedSwingUtility.RecentUsePenalty = 1.0f;
	ChargedSwingUtility.CooldownSeconds = 4.0f;

	DashSwingUtility.PreferredDistance = 700.0f;
	DashSwingUtility.DistanceFalloff = 450.0f;
	DashSwingUtility.FacingWeight = 0.25f;
	DashSwingUtility.RecentUsePenalty = 1.0f;
	DashSwingUtility.CooldownSeconds = 5.0f;
}

void UAshenOathBossDecisionComponent::InitializeDecisionStream(const int32 Seed)
{
	CombatDecisionRandomStream.Initialize(Seed);
	RecentAcceptedAttacks.Reset();
	for (float& LastAcceptedTime : LastAcceptedTimeSeconds)
	{
		LastAcceptedTime = -1.0f;
	}
}

const FAshenOathBossAttackUtilityConfig& UAshenOathBossDecisionComponent::GetUtilityConfig(
	const EAshenOathBossAttackType AttackType) const
{
	switch (AttackType)
	{
	case EAshenOathBossAttackType::SingleSwing:
		return SingleSwingUtility;
	case EAshenOathBossAttackType::Combo:
		return ComboUtility;
	case EAshenOathBossAttackType::ChargedSwing:
		return ChargedSwingUtility;
	case EAshenOathBossAttackType::DashSwing:
		return DashSwingUtility;
	}

	checkNoEntry();
	return SingleSwingUtility;
}

FAshenOathBossScoredAttack UAshenOathBossDecisionComponent::ScoreAttack(
	const FAshenOathBossCombatSnapshot& Snapshot,
	EAshenOathBossAttackType AttackType) const
{
	const FAshenOathBossAttackUtilityConfig& Config = GetUtilityConfig(AttackType);

	FAshenOathBossScoredAttack Result;
	Result.AttackType = AttackType;
	Result.BaseScore = Config.BaseScore;

	const float DistanceError =
		FMath::Abs(Snapshot.Distance2D - Config.PreferredDistance) /
		FMath::Max(1.0f, Config.DistanceFalloff);
	Result.DistancePenalty =
		Config.DistanceWeight * FMath::Clamp(DistanceError, 0.0f, 1.0f);

	Result.FacingPenalty =
		Config.FacingWeight *
		FMath::Clamp(Snapshot.FacingAngleDegrees / 180.0f, 0.0f, 1.0f);

	for (int32 Index = 0; Index < Snapshot.RecentAcceptedAttacks.Num(); ++Index)
	{
		if (Snapshot.RecentAcceptedAttacks[Index] == AttackType)
		{
			Result.RecentPenalty +=
				Config.RecentUsePenalty * (Index == 0 ? 1.0f : 0.5f);
		}
	}

	Result.TotalScore =
		Result.BaseScore -
		Result.DistancePenalty -
		Result.FacingPenalty -
		Result.RecentPenalty;
	return Result;
}

void UAshenOathBossDecisionComponent::BuildRankedAttacks(const FAshenOathBossCombatSnapshot& Snapshot, float MeleeRange, float DashMinStartRange,
	TArray<FAshenOathBossScoredAttack, TInlineAllocator<4>>& OutRanked)
{
	OutRanked.Reset();
	const AAshenOathBossCharacter* Boss =
		CastChecked<AAshenOathBossCharacter>(GetOwner());

	for (const EAshenOathBossAttackType AttackType : Snapshot.AvailableAttacks)
	{
		const bool bDash = AttackType == EAshenOathBossAttackType::DashSwing;
		if (bDash)
		{
			if (Snapshot.Distance2D <= DashMinStartRange ||
				!IsValid(Boss->DashSwingAction) ||
				Snapshot.Distance2D <=
					Boss->DashSwingAction->StopDistance + KINDA_SMALL_NUMBER)
			{
				continue;
			}
		}
		else if (Snapshot.Distance2D > MeleeRange)
		{
			continue;
		}

		const int32 AttackIndex = static_cast<int32>(AttackType);
		check(AttackIndex >= 0 && AttackIndex < BossAttackCount);

		const FAshenOathBossAttackUtilityConfig& Config =
			GetUtilityConfig(AttackType);
		const float LastAccepted =
			Snapshot.LastAcceptedTimeSeconds[AttackIndex];
		if (LastAccepted >= 0.0f &&
			Snapshot.WorldTimeSeconds - LastAccepted < Config.CooldownSeconds)
		{
			continue;
		}

		FAshenOathBossScoredAttack Scored = ScoreAttack(Snapshot, AttackType);
		Scored.TieBreak = CombatDecisionRandomStream.GetFraction();
		OutRanked.Add(Scored);

		UE_LOG(LogAshenOathBossUtility, Verbose,
			TEXT("Attack=%d Score=%.2f Base=%.2f Distance=%.2f Facing=%.2f Recent=%.2f"),
			AttackIndex, Scored.TotalScore, Scored.BaseScore,
			Scored.DistancePenalty, Scored.FacingPenalty,
			Scored.RecentPenalty);
	}

	OutRanked.Sort([](
		const FAshenOathBossScoredAttack& A,
		const FAshenOathBossScoredAttack& B)
	{
		if (A.TotalScore != B.TotalScore)
		{
			return A.TotalScore > B.TotalScore;
		}
		if (A.TieBreak != B.TieBreak)
		{
			return A.TieBreak > B.TieBreak;
		}
		return static_cast<uint8>(A.AttackType) <
			static_cast<uint8>(B.AttackType);
	});
}

void UAshenOathBossDecisionComponent::RecordAcceptedAttack(EAshenOathBossAttackType AttackType)
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const int32 AttackIndex = static_cast<int32>(AttackType);
	check(AttackIndex >= 0 && AttackIndex < BossAttackCount);

	LastAcceptedTimeSeconds[AttackIndex] = World->GetTimeSeconds();
	RecentAcceptedAttacks.Insert(AttackType, 0);
	if (RecentAcceptedAttacks.Num() > 2)
	{
		RecentAcceptedAttacks.SetNum(2);
	}
}

bool UAshenOathBossDecisionComponent::ChooseFirstPhaseCombatIntent(
	AActor* TargetActor,
	const float MeleeRange,
	const float DashMinStartRange)
{
	PendingCombatDecision = {};
	if (!FMath::IsFinite(MeleeRange) || !FMath::IsFinite(DashMinStartRange) ||
		MeleeRange < 0.0f || DashMinStartRange < MeleeRange)
	{
		return false;
	}

	FAshenOathBossCombatSnapshot Snapshot;
	if (!CaptureFirstPhaseCombatSnapshot(TargetActor, Snapshot))
	{
		return false;
	}

	FAshenOathBossCombatDecision Decision;
	Decision.TargetActor = Snapshot.TargetActor;
	Decision.MeleeRange = MeleeRange;
	Decision.DashMinStartRange = DashMinStartRange;
	BuildRankedAttacks(Snapshot, MeleeRange, DashMinStartRange, Decision.RankedAttacks);
	if (!Decision.RankedAttacks.IsEmpty())
	{
		Decision.Intent = EAshenOathBossCombatIntent::Attack;
		const FAshenOathBossScoredAttack& Winner = Decision.RankedAttacks[0];
		UE_LOG(LogAshenOathBossUtility, Verbose,
			TEXT("Decision=Attack Distance=%.0f Facing=%.0f Winner=%d Score=%.2f Base=%.2f DistancePenalty=%.2f FacingPenalty=%.2f RecentPenalty=%.2f"),
			Snapshot.Distance2D, Snapshot.FacingAngleDegrees,
			static_cast<int32>(Winner.AttackType), Winner.TotalScore,
			Winner.BaseScore, Winner.DistancePenalty,
			Winner.FacingPenalty, Winner.RecentPenalty);
	}
	else if (Snapshot.Distance2D > MeleeRange)
	{
		Decision.Intent = EAshenOathBossCombatIntent::Approach;
		Decision.ApproachRange = MeleeRange;
	}
	else
	{
		Decision.Intent = EAshenOathBossCombatIntent::Wait;
	}

	PendingCombatDecision = MoveTemp(Decision);
	return true;
}

bool UAshenOathBossDecisionComponent::CaptureFirstPhaseCombatSnapshot(
	AActor* TargetActor,
	FAshenOathBossCombatSnapshot& OutSnapshot) const
{
	OutSnapshot = {};
	const AAshenOathBossCharacter* Boss = CastChecked<AAshenOathBossCharacter>(GetOwner());
	if (Boss->IsActorBeingDestroyed() ||
		(Boss->DeathComponent && Boss->DeathComponent->IsDeathStarted()) ||
		!IsCurrentCombatTarget(TargetActor))
	{
		return false;
	}

	const UWorld* World = GetWorld();
	OutSnapshot.TargetActor = TargetActor;
	OutSnapshot.Distance2D = FVector::Dist2D(Boss->GetActorLocation(), TargetActor->GetActorLocation());
	if (!FMath::IsFinite(OutSnapshot.Distance2D))
	{
		OutSnapshot = {};
		return false;
	}

	const FVector ToTarget = (TargetActor->GetActorLocation() - Boss->GetActorLocation()).GetSafeNormal2D();
	if (!ToTarget.IsNearlyZero())
	{
		const float FacingDot = FVector::DotProduct(Boss->GetActorForwardVector().GetSafeNormal2D(), ToTarget);
		OutSnapshot.FacingAngleDegrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FacingDot, -1.0f, 1.0f)));
	}

	OutSnapshot.WorldTimeSeconds = World->GetTimeSeconds();
	OutSnapshot.RecentAcceptedAttacks = RecentAcceptedAttacks;
	for (int32 Index = 0; Index < BossAttackCount; ++Index)
	{
		OutSnapshot.LastAcceptedTimeSeconds[Index] = LastAcceptedTimeSeconds[Index];
	}
	for (const EAshenOathBossAttackType AttackType : {
		EAshenOathBossAttackType::SingleSwing,
		EAshenOathBossAttackType::Combo,
		EAshenOathBossAttackType::ChargedSwing,
		EAshenOathBossAttackType::DashSwing
	})
	{
		FGameplayAbilitySpecHandle SpecHandle;
		if (Boss->CanStartBossAttack(AttackType, SpecHandle))
		{
			OutSnapshot.AvailableAttacks.Add(AttackType);
		}
	}
	return true;
}

bool UAshenOathBossDecisionComponent::IsCurrentCombatTarget(const AActor* TargetActor) const
{
	const AAshenOathBossCharacter* Boss = CastChecked<AAshenOathBossCharacter>(GetOwner());
	if (!IsValid(TargetActor) || !GetWorld() || TargetActor->GetWorld() != GetWorld() ||
		UGameplayStatics::GetPlayerPawn(Boss, 0) != TargetActor)
	{
		return false;
	}

	const UCombatDeathComponent* TargetDeath = TargetActor->FindComponentByClass<UCombatDeathComponent>();
	return !TargetDeath || !TargetDeath->IsDeathStarted();
}

EAshenOathBossCombatIntent UAshenOathBossDecisionComponent::GetPendingCombatIntent()
{
	if (PendingCombatDecision.Intent != EAshenOathBossCombatIntent::None &&
		!IsCurrentCombatTarget(PendingCombatDecision.TargetActor.Get()))
	{
		PendingCombatDecision = {};
	}
	return PendingCombatDecision.Intent;
}

void UAshenOathBossDecisionComponent::ClearPendingCombatDecision()
{
	PendingCombatDecision = {};
}

bool UAshenOathBossDecisionComponent::TryConsumeApproachDecision(float& OutRange)
{
	if (PendingCombatDecision.Intent != EAshenOathBossCombatIntent::Approach)
	{
		return false;
	}

	if (!IsCurrentCombatTarget(PendingCombatDecision.TargetActor.Get()))
	{
		PendingCombatDecision = {};
		return false;
	}

	OutRange = PendingCombatDecision.ApproachRange;
	PendingCombatDecision = {};
	return true;
}

FAshenOathBossAttackStartResult UAshenOathBossDecisionComponent::RequestSelectedCombatAttack()
{
	if (PendingCombatDecision.Intent != EAshenOathBossCombatIntent::Attack)
	{
		return {};
	}

	const FAshenOathBossCombatDecision Decision = MoveTemp(PendingCombatDecision);
	PendingCombatDecision = {};
	if (!IsCurrentCombatTarget(Decision.TargetActor.Get()) || Decision.RankedAttacks.IsEmpty())
	{
		return {};
	}

	AAshenOathBossCharacter* Boss = CastChecked<AAshenOathBossCharacter>(GetOwner());
	auto SetFallback = [this, &Decision](const EAshenOathBossCombatIntent Intent)
	{
		// Activation may synchronously trigger a new StateTree decision. Do not
		// overwrite that decision with a fallback from this older request.
		if (PendingCombatDecision.Intent != EAshenOathBossCombatIntent::None ||
			!IsCurrentCombatTarget(Decision.TargetActor.Get()))
		{
			return;
		}

		PendingCombatDecision.TargetActor = Decision.TargetActor;
		PendingCombatDecision.Intent = Intent;
		PendingCombatDecision.ApproachRange = Decision.MeleeRange;
	};

	for (const FAshenOathBossScoredAttack& Candidate : Decision.RankedAttacks)
	{
		if (!IsCurrentCombatTarget(Decision.TargetActor.Get()))
		{
			return {};
		}

		const float CurrentDistance = FVector::Dist2D(
			Boss->GetActorLocation(), Decision.TargetActor->GetActorLocation());
		if (Candidate.AttackType == EAshenOathBossAttackType::DashSwing)
		{
			if (CurrentDistance <= Decision.DashMinStartRange ||
				!IsValid(Boss->DashSwingAction) ||
				CurrentDistance <= Boss->DashSwingAction->StopDistance + KINDA_SMALL_NUMBER)
			{
				SetFallback(CurrentDistance > Decision.MeleeRange
					? EAshenOathBossCombatIntent::Approach
					: EAshenOathBossCombatIntent::Wait);
				return {};
			}
		}
		else if (CurrentDistance > Decision.MeleeRange)
		{
			SetFallback(EAshenOathBossCombatIntent::Approach);
			return {};
		}

		const FAshenOathBossAttackStartResult Result =
			Boss->RequestBossAttack(Candidate.AttackType);
		if (Result.State == EAshenOathBossAttackStartState::Rejected)
		{
			continue;
		}

		if (Result.State == EAshenOathBossAttackStartState::Running ||
			Result.State == EAshenOathBossAttackStartState::Succeeded)
		{
			RecordAcceptedAttack(Candidate.AttackType);
		}
		else
		{
			SetFallback(EAshenOathBossCombatIntent::Wait);
		}
		return Result;
	}

	SetFallback(Decision.RankedAttacks[0].AttackType == EAshenOathBossAttackType::DashSwing
		? EAshenOathBossCombatIntent::Approach
		: EAshenOathBossCombatIntent::Wait);
	return {};
}
