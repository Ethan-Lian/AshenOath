#include "AbilitySystem/Ability/AshenOathHeavyAttackAbility.h"

#include "AbilitySystem/AshenOathAttributeSet.h"
#include "AbilitySystem/AshenOathStaminaRecoveryComponent.h"
#include "AbilitySystem/Data/AshenOathHeavyAttackData.h"
#include "AbilitySystemComponent.h"
#include "ActiveGameplayEffectHandle.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Characters/AshenOathBossCharacter.h"
#include "Characters/AshenOathPlayerCharacter.h"
#include "Feedback/AshenOathCombatFeedbackComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Game/AshenOathGameMode.h"
#include "GameplayEffect.h"
#include "GameplayTags/AshenOathGameplayTags.h"
#include "TimerManager.h"

namespace
{
	constexpr double MinimumChargeDurationSeconds = 1.0;
}

UAshenOathHeavyAttackAbility::UAshenOathHeavyAttackAbility()
{
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(AshenOathGameplayTags::Ability_Action);
	AssetTags.AddTag(AshenOathGameplayTags::Ability_Action_HeavyAttack);
	SetAssetTags(AssetTags);
}

bool UAshenOathHeavyAttackAbility::HandleInputReleased()
{
	if (!IsActive() ||
		ActivePhase != EAshenOathHeavyAttackPhase::Charging)
	{
		return false;
	}
	return StartChargedRelease();
}

FAshenOathChargeState UAshenOathHeavyAttackAbility::GetChargeState() const
{
	FAshenOathChargeState State;
	if (IsActive() && ActivePhase == EAshenOathHeavyAttackPhase::Charging)
	{
		State.bCharging = true;
		State.bCanRelease = GetWorld()->GetTimeSeconds() - ChargeStartedAtSeconds >= MinimumChargeDurationSeconds;
		State.ChargeAlpha = GetChargeAlpha();
	}
	return State;
}

void UAshenOathHeavyAttackAbility::PublishChargeState()
{
	if (UAshenOathCombatFeedbackComponent* Feedback = ChargeFeedbackComponent.Get())
	{
		Feedback->UpdateChargeFeedback(ChargeFeedbackId, GetChargeState());
	}
	if (AAshenOathPlayerCharacter* Player = Cast<AAshenOathPlayerCharacter>(GetAvatarActorFromActorInfo()))
	{
		Player->HeavyChargeChangedEvent.Broadcast();
	}
}

bool UAshenOathHeavyAttackAbility::CancelChargeForDodge()
{
	if (!IsActive() ||
		ActivePhase != EAshenOathHeavyAttackPhase::Charging)
	{
		return false;
	}

	FinishAbility(true);
	return true;
}

bool UAshenOathHeavyAttackAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!IsHeavyDataReady(
			Cast<UAshenOathHeavyAttackData>(ResolveActionData(Handle, ActorInfo))) ||
		!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid() ||
		ActorInfo->AbilitySystemComponent->GetNumericAttribute(
			UAshenOathAttributeSet::GetStaminaAttribute()
		) <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	return Super::CanActivateAbility(
		Handle,
		ActorInfo,
		SourceTags,
		TargetTags,
		OptionalRelevantTags);
}

void UAshenOathHeavyAttackAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	const UAshenOathHeavyAttackData* HeavyData =
		Cast<UAshenOathHeavyAttackData>(ResolveActionData(Handle, ActorInfo));
	UWorld* World = GetWorld();

	if (!IsHeavyDataReady(HeavyData) || !World)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!StartCharging(ActorInfo, HeavyData, World))
	{
		FinishAbility(true);
	}
}

bool UAshenOathHeavyAttackAbility::StartCharging(
	const FGameplayAbilityActorInfo* ActorInfo,
	const UAshenOathHeavyAttackData* HeavyData,
	UWorld* World)
{
	ActiveHeavyData = HeavyData;
	ActivePhase = EAshenOathHeavyAttackPhase::Charging;
	ChargeStartedAtSeconds = World->GetTimeSeconds();
	AccumulatedChargeStaminaCost = 0.0f;
	bReleaseFeedbackPlayed = false;
	AcquireMovementLock();
	if (!IsActive())
	{
		return false;
	}

	if (!StartAttackMontage(
		ActorInfo,
		HeavyData,
		HeavyData->ChargeStartSection))
	{
		return false;
	}

	UAnimInstance* AnimInstance = GetActiveAttackAnimInstance();
	UAnimMontage* Montage = GetActiveAttackMontage();
	if (!AnimInstance || !Montage)
	{
		return false;
	}

	AnimInstance->Montage_SetNextSection(
		HeavyData->ChargeStartSection,
		HeavyData->ChargeLoopSection,
		Montage
	);
	AnimInstance->Montage_SetNextSection(
		HeavyData->ChargeLoopSection,
		HeavyData->ChargeLoopSection,
		Montage
	);

	World->GetTimerManager().SetTimer(
		ChargeDrainTimer,
		this,
		&UAshenOathHeavyAttackAbility::HandleChargeDrainTick,
		HeavyData->ChargeDrainIntervalSeconds,
		true
	);
	World->GetTimerManager().SetTimer(
		MaximumChargeTimer,
		this,
		&UAshenOathHeavyAttackAbility::HandleMaximumChargeReached,
		HeavyData->MaxChargeDurationSeconds,
		false
	);
	World->GetTimerManager().SetTimer(
		ChargeReadyTimer, this, &UAshenOathHeavyAttackAbility::PublishChargeState,
		MinimumChargeDurationSeconds, false);
	if (UAshenOathCombatFeedbackComponent* Feedback =
		GetAvatarActorFromActorInfo()->FindComponentByClass<UAshenOathCombatFeedbackComponent>())
	{
		ChargeFeedbackComponent = Feedback;
		ChargeFeedbackId = Feedback->BeginChargeFeedback(HeavyData);
	}
	PublishChargeState();
	return IsActive();
}

void UAshenOathHeavyAttackAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChargeDrainTimer);
		World->GetTimerManager().ClearTimer(MaximumChargeTimer);
		World->GetTimerManager().ClearTimer(ChargeReadyTimer);
	}

	UAshenOathCombatFeedbackComponent* Feedback = ChargeFeedbackComponent.Get();
	const uint32 FeedbackToEnd = ChargeFeedbackId;
	ChargeFeedbackId = 0;
	ChargeFeedbackComponent.Reset();
	if (Feedback)
	{
		Feedback->EndChargeFeedback(FeedbackToEnd, bWasCancelled);
	}
	ReleaseMovementLock();
	ActiveHeavyData.Reset();
	ActivePhase = EAshenOathHeavyAttackPhase::None;
	ChargeStartedAtSeconds = 0.0;
	AccumulatedChargeStaminaCost = 0.0f;

	Super::EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		bReplicateEndAbility,
		bWasCancelled
	);
	PublishChargeState();
}

bool UAshenOathHeavyAttackAbility::IsHeavyDataReady(
	const UAshenOathHeavyAttackData* HeavyData) const
{
	const UGameplayEffect* ChargeCostEffect = HeavyData && HeavyData->ChargeCostEffect
		? HeavyData->ChargeCostEffect.GetDefaultObject()
		: nullptr;

	if (!HeavyData ||
		HeavyData->MaxChargeDurationSeconds < MinimumChargeDurationSeconds ||
		HeavyData->MaxChargeStaminaCost <= KINDA_SMALL_NUMBER ||
		HeavyData->ChargeDrainIntervalSeconds <= KINDA_SMALL_NUMBER ||
		!ChargeCostEffect ||
		ChargeCostEffect->DurationPolicy != EGameplayEffectDurationType::Instant ||
		HeavyData->NormalAttackDamageMagnitude <= KINDA_SMALL_NUMBER ||
		HeavyData->FullyChargedDamageMultiplier < 1.0f ||
		HeavyData->CostEffect ||
		!HeavyData->StartSection.IsNone() ||
		HeavyData->ChargeStartSection.IsNone() ||
		HeavyData->ChargeLoopSection.IsNone() ||
		HeavyData->ChargedReleaseSection.IsNone() ||
		HeavyData->ReleaseAutoAimMaxAngleDegrees < 0.0f ||
		HeavyData->ReleaseAutoAimMaxAngleDegrees > 180.0f ||
		HeavyData->ReleaseAutoAimMaxDistance <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	return HeavyData->Montage &&
		HeavyData->Montage->IsValidSectionName(HeavyData->ChargeStartSection) &&
		HeavyData->Montage->IsValidSectionName(HeavyData->ChargeLoopSection) &&
		HeavyData->Montage->IsValidSectionName(HeavyData->ChargedReleaseSection);
}

bool UAshenOathHeavyAttackAbility::StartChargedRelease()
{
	if (!IsActive() ||
		ActivePhase != EAshenOathHeavyAttackPhase::Charging)
	{
		return false;
	}

	const UAshenOathHeavyAttackData* HeavyData = ActiveHeavyData.Get();
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UAnimInstance* AnimInstance = GetActiveAttackAnimInstance();
	UAnimMontage* Montage = GetActiveAttackMontage();
	UWorld* World = GetWorld();

	if (!HeavyData || !ActorInfo || !AnimInstance || !Montage || !World)
	{
		FinishAbility(true);
		return false;
	}

	const int32 ChargeMontageInstanceId = GetActiveAttackMontageInstanceId();
	ConsumeChargeThroughCurrentTime();
	if (!IsActive() || ActivePhase != EAshenOathHeavyAttackPhase::Charging ||
		GetActiveAttackMontageInstanceId() != ChargeMontageInstanceId)
	{
		return false;
	}

	// Input release and automatic release share the same minimum hold time.
	// Elapsed charge cost is retained when an early release cancels the action.
	if (World->GetTimeSeconds() - ChargeStartedAtSeconds < MinimumChargeDurationSeconds)
	{
		FinishAbility(true);
		return false;
	}

	World->GetTimerManager().ClearTimer(ChargeDrainTimer);
	World->GetTimerManager().ClearTimer(MaximumChargeTimer);
	World->GetTimerManager().ClearTimer(ChargeReadyTimer);

	const float DamageMagnitude = CalculateChargedDamageMagnitude();

	ActivePhase = EAshenOathHeavyAttackPhase::ChargedRelease;
	AlignChargedReleaseToActiveBoss();
	if (!IsActive() || ActivePhase != EAshenOathHeavyAttackPhase::ChargedRelease ||
		GetActiveAttackMontageInstanceId() != ChargeMontageInstanceId)
	{
		return false;
	}

	if (!BeginMeleeExecution(
		GetCurrentAbilitySpecHandle(),
		ActorInfo,
		GetCurrentActivationInfo(),
		HeavyData,
		EMeleeCostCommit::SkipConfigured,
		AshenOathGameplayTags::Data_Damage,
		DamageMagnitude))
	{
		return false;
	}

	AnimInstance->Montage_JumpToSection(
		HeavyData->ChargedReleaseSection,
		Montage
	);
	if (IsActive() && ActivePhase == EAshenOathHeavyAttackPhase::ChargedRelease)
	{
		PublishChargeState();
	}

	return true;
}

void UAshenOathHeavyAttackAbility::RequestReleaseFeedbackFromAnimation(
	USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation, const int32 MontageInstanceId)
{
	if (ActivePhase != EAshenOathHeavyAttackPhase::ChargedRelease || bReleaseFeedbackPlayed ||
		!IsAttackSignalOwned(Mesh, Animation, MontageInstanceId))
	{
		return;
	}
	bReleaseFeedbackPlayed = true;
	if (UAshenOathCombatFeedbackComponent* Feedback = ChargeFeedbackComponent.Get())
	{
		Feedback->ReleaseChargeFeedback(ChargeFeedbackId, GetChargeAlpha());
	}
}

bool UAshenOathHeavyAttackAbility::ConsumeChargeThroughCurrentTime()
{
	const UAshenOathHeavyAttackData* HeavyData = ActiveHeavyData.Get();
	UWorld* World = GetWorld();
	UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo();
	AActor* AvatarActor = GetAvatarActorFromActorInfo();

	if (ActivePhase != EAshenOathHeavyAttackPhase::Charging || !HeavyData ||
		!World || !AbilitySystem || !IsValid(AvatarActor))
	{
		return false;
	}
	const int32 ChargeMontageInstanceId = GetActiveAttackMontageInstanceId();

	const double ElapsedSeconds = FMath::Clamp(
		World->GetTimeSeconds() - ChargeStartedAtSeconds,
		0.0,
		static_cast<double>(HeavyData->MaxChargeDurationSeconds)
	);
	const float DesiredTotalCost = HeavyData->MaxChargeStaminaCost *
		static_cast<float>(ElapsedSeconds / HeavyData->MaxChargeDurationSeconds);
	const float RequestedCost = FMath::Max(
		DesiredTotalCost - AccumulatedChargeStaminaCost,
		0.0f
	);

	if (RequestedCost <= KINDA_SMALL_NUMBER)
	{
		return true;
	}

	const FGameplayAttribute StaminaAttribute =
		UAshenOathAttributeSet::GetStaminaAttribute();
	const float StaminaBefore = AbilitySystem->GetNumericAttribute(StaminaAttribute);
	const float PayableCost = FMath::Min(RequestedCost, StaminaBefore);

	if (PayableCost <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	FGameplayEffectContextHandle EffectContext = AbilitySystem->MakeEffectContext();
	EffectContext.AddSourceObject(this);
	FGameplayEffectSpecHandle CostSpec = AbilitySystem->MakeOutgoingSpec(
		HeavyData->ChargeCostEffect,
		GetAbilityLevel(),
		EffectContext
	);

	if (!CostSpec.IsValid())
	{
		return false;
	}

	CostSpec.Data->SetSetByCallerMagnitude(
		AshenOathGameplayTags::Data_Cost_Stamina,
		-PayableCost
	);
	const FActiveGameplayEffectHandle AppliedCost =
		AbilitySystem->ApplyGameplayEffectSpecToSelf(*CostSpec.Data.Get());

	if (!AppliedCost.WasSuccessfullyApplied())
	{
		return false;
	}

	const float StaminaAfter = AbilitySystem->GetNumericAttribute(StaminaAttribute);
	const float ActualCost = FMath::Clamp(
		StaminaBefore - StaminaAfter,
		0.0f,
		PayableCost
	);
	const bool bOwnsCharge = IsActive() &&
		ActivePhase == EAshenOathHeavyAttackPhase::Charging &&
		GetActiveAttackMontageInstanceId() == ChargeMontageInstanceId;
	if (bOwnsCharge)
	{
		AccumulatedChargeStaminaCost = FMath::Min(
			AccumulatedChargeStaminaCost + ActualCost,
			HeavyData->MaxChargeStaminaCost
		);
	}

	if (ActualCost > KINDA_SMALL_NUMBER && IsValid(AvatarActor))
	{
		if (UAshenOathStaminaRecoveryComponent* Recovery =
			AvatarActor->FindComponentByClass<UAshenOathStaminaRecoveryComponent>())
		{
			Recovery->NotifyStaminaCostCommitted();
		}
	}

	return bOwnsCharge && IsActive() &&
		GetActiveAttackMontageInstanceId() == ChargeMontageInstanceId &&
		ActualCost + KINDA_SMALL_NUMBER >= RequestedCost &&
		StaminaAfter > KINDA_SMALL_NUMBER;
}

float UAshenOathHeavyAttackAbility::GetChargeAlpha() const
{
	const UAshenOathHeavyAttackData* HeavyData = ActiveHeavyData.Get();
	if (!HeavyData || HeavyData->MaxChargeStaminaCost <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	return FMath::Clamp(
		AccumulatedChargeStaminaCost / HeavyData->MaxChargeStaminaCost,
		0.0f,
		1.0f
	);
}

float UAshenOathHeavyAttackAbility::CalculateChargedDamageMagnitude() const
{
	const UAshenOathHeavyAttackData* HeavyData = ActiveHeavyData.Get();
	if (!HeavyData)
	{
		return 0.0f;
	}
	const float DamageMultiplier = FMath::Lerp(
		1.0f,
		HeavyData->FullyChargedDamageMultiplier,
		GetChargeAlpha()
	);

	// The configured Effect uses an additive Health modifier, so damage is negative.
	return -HeavyData->NormalAttackDamageMagnitude * DamageMultiplier;
}

void UAshenOathHeavyAttackAbility::AlignChargedReleaseToActiveBoss()
{
	const UAshenOathHeavyAttackData* HeavyData = ActiveHeavyData.Get();
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	UWorld* World = GetWorld();

	AAshenOathGameMode* GameMode = World
		? World->GetAuthGameMode<AAshenOathGameMode>()
		: nullptr;

	AAshenOathBossCharacter* Boss = GameMode
		? GameMode->GetActiveBoss()
		: nullptr;

	if (!HeavyData ||
		!IsValid(AvatarActor) ||
		!IsValid(Boss))
	{
		return;
	}

	const UAbilitySystemComponent* BossAbilitySystem =
		Boss->GetAbilitySystemComponent();

	if (!BossAbilitySystem ||
		BossAbilitySystem->HasMatchingGameplayTag(
			AshenOathGameplayTags::State_Dead))
	{
		return;
	}

	FVector SourceOrigin;
	FVector SourceExtent;
	AvatarActor->GetActorBounds(
		true,
		SourceOrigin,
		SourceExtent
	);

	FVector TargetOrigin;
	FVector TargetExtent;
	Boss->GetActorBounds(
		true,
		TargetOrigin,
		TargetExtent
	);

	FVector HorizontalOffset = TargetOrigin - SourceOrigin;
	HorizontalOffset.Z = 0.0f;

	const float DistanceSquared = HorizontalOffset.SizeSquared();
	const float MaximumDistance =
		HeavyData->ReleaseAutoAimMaxDistance;

	if (DistanceSquared <= KINDA_SMALL_NUMBER ||
		DistanceSquared > FMath::Square(MaximumDistance))
	{
		return;
	}

	const FVector TargetDirection =
		HorizontalOffset.GetSafeNormal();
	const FVector CurrentForward =
		AvatarActor->GetActorForwardVector().GetSafeNormal2D();

	const float MinimumForwardDot = FMath::Cos(
		FMath::DegreesToRadians(
			HeavyData->ReleaseAutoAimMaxAngleDegrees
		)
	);

	if (FVector::DotProduct(CurrentForward, TargetDirection) <
		MinimumForwardDot)
	{
		return;
	}

	const FVector VisibilityStart =
		SourceOrigin + FVector(0.0f, 0.0f, SourceExtent.Z * 0.25f);
	const FVector VisibilityEnd =
		TargetOrigin + FVector(0.0f, 0.0f, TargetExtent.Z * 0.1f);

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(AshenOathChargedReleaseAim),
		false,
		AvatarActor
	);
	FHitResult BlockingHit;

	if (World->LineTraceSingleByChannel(
			BlockingHit,
			VisibilityStart,
			VisibilityEnd,
			ECC_Visibility,
			QueryParams) &&
		BlockingHit.GetActor() != Boss)
	{
		return;
	}

	const FRotator ReleaseRotation(
		0.0f,
		TargetDirection.Rotation().Yaw,
		0.0f
	);
	AvatarActor->SetActorRotation(
		ReleaseRotation,
		ETeleportType::None
	);
}

void UAshenOathHeavyAttackAbility::HandleChargeDrainTick()
{
	if (!IsActive() ||
		ActivePhase != EAshenOathHeavyAttackPhase::Charging)
	{
		return;
	}

	if (!ConsumeChargeThroughCurrentTime())
	{
		StartChargedRelease();
	}
	else
	{
		PublishChargeState();
	}
}

void UAshenOathHeavyAttackAbility::HandleMaximumChargeReached()
{
	if (!IsActive() ||
		ActivePhase != EAshenOathHeavyAttackPhase::Charging)
	{
		return;
	}

	ConsumeChargeThroughCurrentTime();
	StartChargedRelease();
}

void UAshenOathHeavyAttackAbility::AcquireMovementLock()
{
	UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo();
	if (bOwnsMovementLock || !AbilitySystem)
	{
		return;
	}

	bOwnsMovementLock = true;
	AbilitySystem->AddLooseGameplayTag(AshenOathGameplayTags::State_MovementLocked);
}

void UAshenOathHeavyAttackAbility::ReleaseMovementLock()
{
	UAbilitySystemComponent* AbilitySystem = GetAbilitySystemComponentFromActorInfo();
	if (!bOwnsMovementLock)
	{
		return;
	}

	bOwnsMovementLock = false;
	if (AbilitySystem)
	{
		AbilitySystem->RemoveLooseGameplayTag(AshenOathGameplayTags::State_MovementLocked);
	}
}
