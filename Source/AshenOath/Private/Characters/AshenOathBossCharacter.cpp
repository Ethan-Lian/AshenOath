#include "Characters/AshenOathBossCharacter.h"

#include "AIController.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Ability/AshenOathBossSingleSwingAbility.h"
#include "AbilitySystem/Ability/AshenOathBossComboAbility.h"
#include "AbilitySystem/Ability/AshenOathBossChargedSwingAbility.h"
#include "AbilitySystem/Ability/AshenOathBossDashSwingAbility.h"
#include "AbilitySystem/AshenOathAttributeSet.h"
#include "AbilitySystem/Data/AshenOathMeleeActionData.h"
#include "AbilitySystem/Data/AshenOathBossComboActionData.h"
#include "AbilitySystem/Data/AshenOathBossChargedSwingActionData.h"
#include "AbilitySystem/Data/AshenOathBossDashSwingActionData.h"
#include "Actions/CombatMeleeComponent.h"
#include "Reaction/CombatHitReactionComponent.h"
#include "Death/CombatDeathComponent.h"
#include "Components/StateTreeComponent.h"
#include "Damage/CombatDamageComponent.h"
#include "Game/AshenOathGameMode.h"
#include "GameplayEffect.h"
#include "GameplayTags/AshenOathGameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
	constexpr EAshenOathBossAttackType NearAttacks[] = {
		EAshenOathBossAttackType::SingleSwing,
		EAshenOathBossAttackType::Combo,
		EAshenOathBossAttackType::ChargedSwing
	};
}

AAshenOathBossCharacter::AAshenOathBossCharacter()
{
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));

	AbilitySystemComponent->SetIsReplicated(false);

	AttributeSet = CreateDefaultSubobject<UAshenOathAttributeSet>(TEXT("AttributeSet"));

	StateTreeComponent = CreateDefaultSubobject<UStateTreeComponent>(TEXT("StateTreeComponent"));

	CombatDamageComponent = CreateDefaultSubobject<UCombatDamageComponent>(TEXT("CombatDamageComponent"));
	CombatDamageComponent->ConfigureInvulnerabilityTag(AshenOathGameplayTags::State_Invulnerable);
	CombatMeleeComponent = CreateDefaultSubobject<UCombatMeleeComponent>(TEXT("CombatMeleeComponent"));
	HitReactionComponent = CreateDefaultSubobject<UCombatHitReactionComponent>(
		TEXT("HitReactionComponent")
	);
	HitReactionComponent->ConfigureGameplayTags(
		AshenOathGameplayTags::State_Staggered,
		AshenOathGameplayTags::State_Dead,
		AshenOathGameplayTags::Ability_Action
	);
	DeathComponent = CreateDefaultSubobject<UCombatDeathComponent>(TEXT("DeathComponent"));
	DeathComponent->ConfigureDeathContract(
		UAshenOathAttributeSet::GetHealthAttribute(),
		AshenOathGameplayTags::State_Dead
	);

	SingleSwingAbilityClass = UAshenOathBossSingleSwingAbility::StaticClass();
	ComboAbilityClass = UAshenOathBossComboAbility::StaticClass();
	ChargedSwingAbilityClass = UAshenOathBossChargedSwingAbility::StaticClass();
	DashSwingAbilityClass = UAshenOathBossDashSwingAbility::StaticClass();

	StateTreeComponent->SetStartLogicAutomatically(false);
}

void AAshenOathBossCharacter::BeginPlay()
{
	Super::BeginPlay();

	check(AbilitySystemComponent);
	check(AttributeSet);
	check(StateTreeComponent);
	CombatDecisionRandomStream.Initialize(CombatDecisionSeed);

	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	AbilityEndedDelegateHandle = AbilitySystemComponent->OnAbilityEnded.AddUObject(
		this,
		&AAshenOathBossCharacter::HandleAbilityEnded
	);
	DeathStartedHandle = DeathComponent->OnDeathStarted().AddUObject(
		this,
		&AAshenOathBossCharacter::HandleDeathStarted
	);
	ApplyInitialAttributes();

	if (!bInitialAttributesApplied)
	{
		return;
	}

	CombatDamageComponent->ConfigureTerminalStateTag(AshenOathGameplayTags::State_Dead);

	DeathComponent->Initialize(AbilitySystemComponent);

	if (DeathComponent->IsDeathStarted())
	{
		return;
	}

	if (!GrantConfiguredAbilities())
	{
		return;
	}

	UWorld* World = GetWorld();

	AAshenOathGameMode* GameMode = World ? World->GetAuthGameMode<AAshenOathGameMode>() : nullptr;

	// Register the boss so GameMode can notify the UI system to build the corresponding boss UI.
	if (!IsValid(GameMode) || !GameMode->RegisterBoss(this))
	{
		return;
	}
	StateTreeComponent->StartLogic();
}

void AAshenOathBossCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (DeathComponent && DeathStartedHandle.IsValid())
	{
		DeathComponent->OnDeathStarted().Remove(DeathStartedHandle);
		DeathStartedHandle.Reset();
	}

	if (StateTreeComponent)
	{
		StateTreeComponent->StopLogic(TEXT("Boss EndPlay"));
	}
	PendingCombatDecision = {};

	CancelCombatAbilities();

	if (ActiveBossAttackRequest.IsValid())
	{
		FinishBossAttackRequest(true);
	}

	if (AbilitySystemComponent && AbilityEndedDelegateHandle.IsValid())
	{
		AbilitySystemComponent->OnAbilityEnded.Remove(AbilityEndedDelegateHandle);
		AbilityEndedDelegateHandle.Reset();
	}

	SingleSwingAbilitySpecHandle = FGameplayAbilitySpecHandle();
	ComboAbilitySpecHandle = FGameplayAbilitySpecHandle();
	ChargedSwingAbilitySpecHandle = FGameplayAbilitySpecHandle();
	DashSwingAbilitySpecHandle = FGameplayAbilitySpecHandle();
	SingleSwingEndedEvent.Clear();
	BossAttackEndedEvent.Clear();

	// unregister boss so the UI can remove the corresponding boss UI.
	if (UWorld* World = GetWorld())
	{
		if (AAshenOathGameMode* GameMode = World->GetAuthGameMode<AAshenOathGameMode>())
		{
			GameMode->UnregisterBoss(this);
		}
	}

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->ClearActorInfo();
	}

	Super::EndPlay(EndPlayReason);
}

void AAshenOathBossCharacter::ApplyInitialAttributes()
{
	if (bInitialAttributesApplied || !InitialAttributesEffect)
	{
		return;
	}

	FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	const FGameplayEffectSpecHandle EffectSpec =
	    AbilitySystemComponent->MakeOutgoingSpec(InitialAttributesEffect, 1.0f, EffectContext);

	if (!EffectSpec.IsValid())
	{
		return;
	}

	const FActiveGameplayEffectHandle AppliedHandle =
	    AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*EffectSpec.Data.Get());

	if (AppliedHandle.WasSuccessfullyApplied())
	{
		bInitialAttributesApplied = true;
	}
}

UAbilitySystemComponent* AAshenOathBossCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

const UAshenOathAttributeSet* AAshenOathBossCharacter::GetAttributeSet() const
{
	return AttributeSet;
}

bool AAshenOathBossCharacter::RequestSingleSwing()
{
	return AbilitySystemComponent &&
		SingleSwingAbilitySpecHandle.IsValid() &&
		AbilitySystemComponent->TryActivateAbility(SingleSwingAbilitySpecHandle);
}

bool AAshenOathBossCharacter::IsSingleSwingActive() const
{
	const FGameplayAbilitySpec* Spec = AbilitySystemComponent && SingleSwingAbilitySpecHandle.IsValid()
		? AbilitySystemComponent->FindAbilitySpecFromHandle(SingleSwingAbilitySpecHandle)
		: nullptr;

	return Spec && Spec->IsActive();
}

void AAshenOathBossCharacter::CancelSingleSwing()
{
	if (AbilitySystemComponent && IsSingleSwingActive())
	{
		AbilitySystemComponent->CancelAbilityHandle(SingleSwingAbilitySpecHandle);
	}
}

#if WITH_DEV_AUTOMATION_TESTS
void AAshenOathBossCharacter::GrantSingleSwingForTesting(UAshenOathMeleeActionData* ActionData)
{
	if (!IsValid(ActionData) || SingleSwingAbilitySpecHandle.IsValid())
	{
		return;
	}

	SingleSwingAction = ActionData;
	GrantConfiguredAbilities();
}
#endif

bool AAshenOathBossCharacter::GrantConfiguredAbilities()
{
	if (!GrantAbilityIfNeeded(
		SingleSwingAbilityClass,
		SingleSwingAction,
		SingleSwingAbilitySpecHandle))
	{
		return false;
	}

	// Optional attack assets can be granted independently without changing the
	// existing single-swing startup contract.
	return (!ComboAction || GrantAbilityIfNeeded(
			ComboAbilityClass, ComboAction, ComboAbilitySpecHandle)) &&
		(!ChargedSwingAction || GrantAbilityIfNeeded(
			ChargedSwingAbilityClass, ChargedSwingAction, ChargedSwingAbilitySpecHandle)) &&
		(!DashSwingAction || GrantAbilityIfNeeded(
			DashSwingAbilityClass, DashSwingAction, DashSwingAbilitySpecHandle));
}

bool AAshenOathBossCharacter::GrantAbilityIfNeeded(
	const TSubclassOf<UGameplayAbility> AbilityClass,
	UAshenOathMeleeActionData* ActionData,
	FGameplayAbilitySpecHandle& InOutHandle)
{
	if (InOutHandle.IsValid())
	{
		return true;
	}

	if (!AbilitySystemComponent || !AbilityClass || !IsValid(ActionData))
	{
		return false;
	}

	InOutHandle = AbilitySystemComponent->GiveAbility(
		FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, ActionData)
	);
	return InOutHandle.IsValid();
}

void AAshenOathBossCharacter::CancelCombatAbilities()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	FGameplayTagContainer AbilityTags;
	AbilityTags.AddTag(AshenOathGameplayTags::Ability_Action);
	AbilitySystemComponent->CancelAbilities(&AbilityTags);
}

void AAshenOathBossCharacter::HandleAbilityEnded(const FAbilityEndedData& EndedData)
{
	const bool bOwnedRequestEnded =
		ActiveBossAttackRequest.IsValid() &&
		EndedData.AbilitySpecHandle == ActiveBossAttackSpec;

	if (bOwnedRequestEnded)
	{
		FinishBossAttackRequest(EndedData.bWasCancelled);
	}

	// Keep the current single-swing StateTree path working until its task is
	// migrated to the request-handle interface.
	if (!bOwnedRequestEnded &&
		EndedData.AbilitySpecHandle == SingleSwingAbilitySpecHandle)
	{
		SingleSwingEndedEvent.Broadcast(EndedData.bWasCancelled);
	}
}

void AAshenOathBossCharacter::HandleDeathStarted()
{
	if (StateTreeComponent)
	{
		StateTreeComponent->StopLogic(TEXT("Boss died"));
	}
	PendingCombatDecision = {};

	CancelCombatAbilities();

	if (ActiveBossAttackRequest.IsValid())
	{
		FinishBossAttackRequest(true);
	}

	if (CombatMeleeComponent)
	{
		CombatMeleeComponent->ResetCombatState();
	}

	if (HitReactionComponent)
	{
		HitReactionComponent->ResetReaction();
	}

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}

	if (UWorld* World = GetWorld())
	{
		if (AAshenOathGameMode* GameMode =
			World->GetAuthGameMode<AAshenOathGameMode>())
		{
			GameMode->ReportBossDeath(this);
		}
	}
}

FAshenOathBossAttackStartResult AAshenOathBossCharacter::RequestBossAttack(EAshenOathBossAttackType AttackType)
{
	FGameplayAbilitySpecHandle SpecHandle;
	if (!CanStartBossAttack(AttackType, SpecHandle))
	{
		return {};
	}

	const FAshenOathBossAttackRequestHandle Request =
		ReserveBossAttackRequest(SpecHandle);
	const bool bAccepted = AbilitySystemComponent->TryActivateAbility(SpecHandle);
	return ResolveBossAttackStart(Request, SpecHandle, bAccepted);
}

bool AAshenOathBossCharacter::CanStartBossAttack(
	const EAshenOathBossAttackType AttackType,
	FGameplayAbilitySpecHandle& OutSpecHandle) const
{
	if (!AbilitySystemComponent ||
			IsActorBeingDestroyed() ||
			bStartingBossAttack ||
			ActiveBossAttackRequest.IsValid() ||
			AbilitySystemComponent->HasMatchingGameplayTag(
				AshenOathGameplayTags::State_Dead) ||
			AbilitySystemComponent->HasMatchingGameplayTag(
				AshenOathGameplayTags::State_Staggered))
	{
		return false;
	}

	OutSpecHandle = ResolveBossAttackSpec(AttackType);

	const FGameplayAbilitySpec* Spec = OutSpecHandle.IsValid()
		? AbilitySystemComponent->FindAbilitySpecFromHandle(OutSpecHandle)
		: nullptr;

	return Spec && !Spec->IsActive();
}

FAshenOathBossAttackRequestHandle AAshenOathBossCharacter::ReserveBossAttackRequest(
	const FGameplayAbilitySpecHandle SpecHandle)
{
	// Zero is reserved for an invalid request handle.
	if (NextBossAttackRequestValue == 0)
	{
		++NextBossAttackRequestValue;
	}

	const FAshenOathBossAttackRequestHandle Request{
		NextBossAttackRequestValue++
	};

	// Record ownership before TryActivateAbility: activation or a Cost callback
	// may synchronously end the Ability.
	ActiveBossAttackRequest = Request;
	ActiveBossAttackSpec = SpecHandle;
	bStartingBossAttack = true;
	bSynchronousBossAttackEnded = false;
	bSynchronousBossAttackWasCancelled = false;
	bCancelBossAttackAfterStart = false;

	return Request;
}

FAshenOathBossAttackStartResult AAshenOathBossCharacter::ResolveBossAttackStart(
	const FAshenOathBossAttackRequestHandle Request,
	const FGameplayAbilitySpecHandle SpecHandle,
	const bool bAccepted)
{
	if (bCancelBossAttackAfterStart &&
		ActiveBossAttackRequest == Request)
	{
		const FGameplayAbilitySpec* RunningSpec =
			AbilitySystemComponent->FindAbilitySpecFromHandle(SpecHandle);

		if (RunningSpec && RunningSpec->IsActive())
		{
			AbilitySystemComponent->CancelAbilityHandle(SpecHandle);
		}

		if (ActiveBossAttackRequest == Request)
		{
			FinishBossAttackRequest(true);
		}
	}

	bStartingBossAttack = false;
	bCancelBossAttackAfterStart = false;

	if (bSynchronousBossAttackEnded)
	{
		return {
			bSynchronousBossAttackWasCancelled
				? EAshenOathBossAttackStartState::Failed
				: EAshenOathBossAttackStartState::Succeeded,
			Request
		};
	}

	if (!bAccepted)
	{
		ActiveBossAttackRequest = {};
		ActiveBossAttackSpec = {};
		return {};
	}

	const FGameplayAbilitySpec* RunningSpec =
		AbilitySystemComponent->FindAbilitySpecFromHandle(SpecHandle);

	if (!RunningSpec ||
		!RunningSpec->IsActive() ||
		!(ActiveBossAttackRequest == Request))
	{
		ActiveBossAttackRequest = {};
		ActiveBossAttackSpec = {};
		return {EAshenOathBossAttackStartState::Failed, Request};
	}

	return {EAshenOathBossAttackStartState::Running, Request};
}

FAshenOathBossAttackStartResult AAshenOathBossCharacter::RequestFirstPhaseAttack(
	const float TargetDistance,
	const float DashMinRange)
{
	if (TargetDistance > DashMinRange && !bLastAttackWasDash)
	{
		const FAshenOathBossAttackStartResult DashResult =
			RequestBossAttack(EAshenOathBossAttackType::DashSwing);
		if (DashResult.State != EAshenOathBossAttackStartState::Rejected)
		{
			bLastAttackWasDash = true;
			return DashResult;
		}
	}

	constexpr int32 NumNearAttacks = UE_ARRAY_COUNT(NearAttacks);
	for (int32 Offset = 0; Offset < NumNearAttacks; ++Offset)
	{
		const int32 Index = (NextNearAttackIndex + Offset) % NumNearAttacks;
		const FAshenOathBossAttackStartResult Result =
			RequestBossAttack(NearAttacks[Index]);
		if (Result.State != EAshenOathBossAttackStartState::Rejected)
		{
			NextNearAttackIndex = (Index + 1) % NumNearAttacks;
			bLastAttackWasDash = false;
			return Result;
		}
	}

	return {};
}

bool AAshenOathBossCharacter::ChooseFirstPhaseCombatIntent(
	const float TargetDistance,
	const float MeleeRange,
	const float DashMinStartRange,
	const float DashProbability)
{
	PendingCombatDecision = {};
	if (!FMath::IsFinite(TargetDistance) || MeleeRange < 0.0f ||
		DashMinStartRange < MeleeRange || !FMath::IsFinite(DashProbability))
	{
		return false;
	}

	const float Distance = FMath::Max(0.0f, TargetDistance);
	PendingCombatDecision.MeleeRange = MeleeRange;
	if (Distance > MeleeRange)
	{
		const bool bDashCanReach = DashSwingAbilitySpecHandle.IsValid() &&
			IsValid(DashSwingAction) &&
			Distance > DashSwingAction->StopDistance + KINDA_SMALL_NUMBER;
		if (Distance > DashMinStartRange && bDashCanReach && !bLastAttackWasDash &&
			CombatDecisionRandomStream.GetFraction() < FMath::Clamp(DashProbability, 0.0f, 1.0f))
		{
			PendingCombatDecision.Intent = EAshenOathBossCombatIntent::Attack;
			PendingCombatDecision.AttackType = EAshenOathBossAttackType::DashSwing;
			return true;
		}

		PendingCombatDecision.Intent = EAshenOathBossCombatIntent::Approach;
		PendingCombatDecision.ApproachRange = MeleeRange;
		return true;
	}

	for (int32 Offset = 0; Offset < UE_ARRAY_COUNT(NearAttacks); ++Offset)
	{
		const int32 Index = (NextNearAttackIndex + Offset) % UE_ARRAY_COUNT(NearAttacks);
		if (ResolveBossAttackSpec(NearAttacks[Index]).IsValid())
		{
			PendingCombatDecision.Intent = EAshenOathBossCombatIntent::Attack;
			PendingCombatDecision.AttackType = NearAttacks[Index];
			return true;
		}
	}

	PendingCombatDecision.Intent = EAshenOathBossCombatIntent::Wait;
	return true;
}

EAshenOathBossCombatIntent AAshenOathBossCharacter::GetPendingCombatIntent() const
{
	return PendingCombatDecision.Intent;
}

void AAshenOathBossCharacter::ClearPendingCombatDecision()
{
	PendingCombatDecision = {};
}

bool AAshenOathBossCharacter::TryConsumeApproachDecision(float& OutRange)
{
	if (PendingCombatDecision.Intent != EAshenOathBossCombatIntent::Approach)
	{
		return false;
	}

	OutRange = PendingCombatDecision.ApproachRange;
	PendingCombatDecision = {};
	return true;
}

FAshenOathBossAttackStartResult AAshenOathBossCharacter::RequestSelectedCombatAttack()
{
	if (PendingCombatDecision.Intent != EAshenOathBossCombatIntent::Attack)
	{
		return {};
	}

	const FAshenOathBossCombatDecision Decision = PendingCombatDecision;
	PendingCombatDecision = {};
	return Decision.AttackType == EAshenOathBossAttackType::DashSwing
		? RequestSelectedDashAttack(Decision)
		: RequestSelectedNearAttack(Decision);
}

FAshenOathBossAttackStartResult AAshenOathBossCharacter::RequestSelectedDashAttack(
	const FAshenOathBossCombatDecision& Decision)
{
	const FAshenOathBossAttackStartResult Result =
		RequestBossAttack(EAshenOathBossAttackType::DashSwing);
	if (Result.State == EAshenOathBossAttackStartState::Rejected)
	{
		PendingCombatDecision.Intent = EAshenOathBossCombatIntent::Approach;
		PendingCombatDecision.ApproachRange = Decision.MeleeRange;
	}
	else if (Result.State == EAshenOathBossAttackStartState::Running ||
		Result.State == EAshenOathBossAttackStartState::Succeeded)
	{
		bLastAttackWasDash = true;
	}
	else
	{
		PendingCombatDecision.Intent = EAshenOathBossCombatIntent::Wait;
	}
	return Result;
}

FAshenOathBossAttackStartResult AAshenOathBossCharacter::RequestSelectedNearAttack(
	const FAshenOathBossCombatDecision& Decision)
{
	int32 FirstIndex = INDEX_NONE;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(NearAttacks); ++Index)
	{
		if (NearAttacks[Index] == Decision.AttackType)
		{
			FirstIndex = Index;
			break;
		}
	}

	if (FirstIndex == INDEX_NONE)
	{
		PendingCombatDecision.Intent = EAshenOathBossCombatIntent::Wait;
		return {};
	}

	for (int32 Offset = 0; Offset < UE_ARRAY_COUNT(NearAttacks); ++Offset)
	{
		const int32 Index = (FirstIndex + Offset) % UE_ARRAY_COUNT(NearAttacks);
		const FAshenOathBossAttackStartResult Result = RequestBossAttack(NearAttacks[Index]);
		if (Result.State == EAshenOathBossAttackStartState::Rejected)
		{
			continue;
		}

		if (Result.State == EAshenOathBossAttackStartState::Running ||
			Result.State == EAshenOathBossAttackStartState::Succeeded)
		{
			NextNearAttackIndex = (Index + 1) % UE_ARRAY_COUNT(NearAttacks);
			bLastAttackWasDash = false;
		}
		else
		{
			PendingCombatDecision.Intent = EAshenOathBossCombatIntent::Wait;
		}
		return Result;
	}

	PendingCombatDecision.Intent = EAshenOathBossCombatIntent::Wait;
	return {};
}

bool AAshenOathBossCharacter::CancelBossAttack(FAshenOathBossAttackRequestHandle Request)
{
	if (!Request.IsValid() ||
		!(ActiveBossAttackRequest == Request) ||
		!AbilitySystemComponent)
	{
		return false;
	}

	if (bStartingBossAttack)
	{
		// Activation may not yet have made the spec active. Complete the
		// cancellation after TryActivateAbility returns.
		bCancelBossAttackAfterStart = true;
		return true;
	}

	const FGameplayAbilitySpec* Spec =
		AbilitySystemComponent->FindAbilitySpecFromHandle(
			ActiveBossAttackSpec
		);

	if (Spec && Spec->IsActive())
	{
		AbilitySystemComponent->CancelAbilityHandle(
			ActiveBossAttackSpec
		);
	}

	// OnAbilityEnded normally finishes the request synchronously. This
	// fallback covers an inactive spec that produced no end callback.
	if (ActiveBossAttackRequest == Request)
	{
		FinishBossAttackRequest(true);
	}

	return true;
}

FGameplayAbilitySpecHandle AAshenOathBossCharacter::ResolveBossAttackSpec(EAshenOathBossAttackType AttackType) const
{
	switch (AttackType)
	{
	case EAshenOathBossAttackType::SingleSwing:
		return SingleSwingAbilitySpecHandle;

	case EAshenOathBossAttackType::Combo:
		return ComboAbilitySpecHandle;

	case EAshenOathBossAttackType::ChargedSwing:
		return ChargedSwingAbilitySpecHandle;

	case EAshenOathBossAttackType::DashSwing:
		return DashSwingAbilitySpecHandle;
	}

	return FGameplayAbilitySpecHandle();
}

void AAshenOathBossCharacter::FinishBossAttackRequest(bool bWasCancelled)
{
	if (!ActiveBossAttackRequest.IsValid())
	{
		return;
	}

	const FAshenOathBossAttackRequestHandle FinishedRequest =
		ActiveBossAttackRequest;

	// Revoke ownership before notifying observers. A callback may immediately
	// request the next attack or make the StateTree exit this task.
	ActiveBossAttackRequest = {};
	ActiveBossAttackSpec = {};

	if (bStartingBossAttack)
	{
		bSynchronousBossAttackEnded = true;
		bSynchronousBossAttackWasCancelled = bWasCancelled;
		return;
	}

	BossAttackEndedEvent.Broadcast(FinishedRequest, bWasCancelled);
}
