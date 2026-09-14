#include "Actions/CombatActionComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Actions/CombatActionData.h"
#include "Actions/CombatMeleeComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "TimerManager.h"

UCombatActionComponent::UCombatActionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UCombatActionComponent::BeginPlay()
{
	Super::BeginPlay();

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	CachedCharacter = Character;

	if (IsValid(Character))
	{
		MeleeComponent = Character->FindComponentByClass<UCombatMeleeComponent>();
	}
}

void UCombatActionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelCurrentAction(0.0f);
	StopResourceRecovery();
	MeleeComponent.Reset();
	CachedCharacter.Reset();

	Super::EndPlay(EndPlayReason);
}

ECombatActionStartResult UCombatActionComponent::TryStartAction(const UCombatActionData* ActionData,
	                                                            FCombatActionHandle& OutHandle)
{
	return TryStartAction(ActionData, FVector::ZeroVector, OutHandle);
}

ECombatActionStartResult UCombatActionComponent::TryStartAction(const UCombatActionData* ActionData,
	                                                            const FVector& MovementDirection,
	                                                            FCombatActionHandle& OutHandle)
{
	// OutHandle belongs to the caller and may contain a handle from an earlier request.
	// Reset it first so a rejected request always returns an invalid handle.
	// A successful request will replace it with the new action handle.
	OutHandle.Reset();

	if (CurrentAction.IsValid())
	{
		return ECombatActionStartResult::RejectedAlreadyActive;
	}

	ACharacter* Character = CachedCharacter.Get();

	// Dynamically spawned actors can receive a request in the same frame their
	// components enter play. Resolve the owner lazily as well as in BeginPlay.
	if (!Character)
	{
		Character = Cast<ACharacter>(GetOwner());
		CachedCharacter = Character;

		if (Character && !MeleeComponent.IsValid())
		{
			MeleeComponent = Character->FindComponentByClass<UCombatMeleeComponent>();
		}
	}

	if (!Character || Character->IsActorBeingDestroyed())
	{
		return ECombatActionStartResult::RejectedInvalidOwner;
	}

	if (!ActionData || !ActionData->Montage || ActionData->PlayRate <= 0.0f ||
	    !IsActionDataValid(*ActionData, MovementDirection))
	{
		return ECombatActionStartResult::RejectedInvalidData;
	}

	if (!ActionData->StartSection.IsNone() &&
	    ActionData->Montage->GetSectionIndex(ActionData->StartSection) == INDEX_NONE)
	{
		return ECombatActionStartResult::RejectedInvalidData;
	}

	USkeletalMeshComponent* MeshComponent = Character->GetMesh();
	UAnimInstance* AnimInstance = MeshComponent ? MeshComponent->GetAnimInstance() : nullptr;

	if (!AnimInstance)
	{
		return ECombatActionStartResult::RejectedInvalidAnimation;
	}

	const bool bHasMeleeConfiguration =
		ActionData->DamageEffect || !ActionData->MeleeTraceBones.IsEmpty();
	UCombatMeleeComponent* Melee = MeleeComponent.Get();

	if (bHasMeleeConfiguration &&
		(!IsValid(Melee) ||
			!Melee->CanStartSession(
				ActionData->DamageEffect,
				ActionData->MeleeTraceRadius,
				ActionData->MeleeTraceBones,
				MeshComponent,
				AnimInstance)))
	{
		return ECombatActionStartResult::RejectedInvalidData;
	}

	UAbilitySystemComponent* AbilitySystemComponent = ResolveAbilitySystemComponent();
	FGameplayEffectSpecHandle CostSpec;

	if (!ActionData->WindowTags.IsEmpty() && !AbilitySystemComponent)
	{
		return ECombatActionStartResult::RejectedInvalidOwner;
	}

	// A null CostEffect deliberately means that this action is free. When a cost
	// is configured, validate and prepare it before starting any presentation so
	// an unaffordable request has no animation or gameplay side effects.
	if (ActionData->CostEffect)
	{
		const UGameplayEffect* CostEffect = ActionData->CostEffect.GetDefaultObject();

		// Action costs are one-time transactions. Duration or infinite effects
		// would turn a stamina payment into persistent mutable state.
		if (!AbilitySystemComponent || !CostEffect ||
		    CostEffect->DurationPolicy != EGameplayEffectDurationType::Instant)
		{
			return ECombatActionStartResult::RejectedInvalidCost;
		}

		FGameplayEffectContextHandle CostContext = AbilitySystemComponent->MakeEffectContext();
		CostContext.AddSourceObject(Character);

		CostSpec = AbilitySystemComponent->MakeOutgoingSpec(ActionData->CostEffect, 1.0f, CostContext);

		if (!CostSpec.IsValid())
		{
			return ECombatActionStartResult::RejectedInvalidCost;
		}

		// UE evaluates the additive modifiers against current attributes. A cost
		// that would reduce an attribute below zero is rejected before Montage_Play.
		if (!AbilitySystemComponent->CanApplyAttributeModifiers(CostEffect, 1.0f, CostContext))
		{
			return ECombatActionStartResult::RejectedInsufficientResources;
		}
	}

	const float PlayedDuration = AnimInstance->Montage_Play(ActionData->Montage, ActionData->PlayRate);

	if (PlayedDuration <= 0.0f)
	{
		return ECombatActionStartResult::RejectedMontageFailed;
	}

	CurrentAction.Value = AllocateActionInstanceId();
	ActiveAnimInstance = AnimInstance;
	ActiveMontage = ActionData->Montage;
	const FCombatActionHandle StartedAction = CurrentAction;
	const FAnimMontageInstance* MontageInstance =
		AnimInstance->GetActiveInstanceForMontage(ActionData->Montage);

	if (!MontageInstance)
	{
		FinishAction(StartedAction.Value, true, 0.0f);
		return ECombatActionStartResult::RejectedMontageFailed;
	}

	if (bHasMeleeConfiguration)
	{
		ActiveMeleeSession = Melee->BeginSession(
			ActionData->DamageEffect,
			ActionData->MeleeTraceRadius,
			ActionData->MeleeTraceBones,
			ActionData->bCanTriggerPerfectDodge,
			MeshComponent,
			AnimInstance,
			ActionData->Montage,
			MontageInstance->GetInstanceID()
		);

		if (!ActiveMeleeSession.IsValid())
		{
			FinishAction(StartedAction.Value, true, 0.0f);
			return ECombatActionStartResult::RejectedInvalidData;
		}
	}

	if (!ActionData->StartSection.IsNone())
	{
		AnimInstance->Montage_JumpToSection(ActionData->StartSection, ActionData->Montage);
	}

	FOnMontageEnded EndDelegate;

	// FOnMontageEnded supplies Montage and bInterrupted. BindUObject stores
	// this execution ID as an additional payload for stale-callback checks.
	EndDelegate.BindUObject(this, &UCombatActionComponent::HandleMontageEnded, CurrentAction.Value);

	AnimInstance->Montage_SetEndDelegate(EndDelegate, ActionData->Montage);

	if (CostSpec.IsValid())
	{
		if (!IsValid(AbilitySystemComponent))
		{
			FinishAction(StartedAction.Value, true, 0.0f);
			return ECombatActionStartResult::RejectedCostApplicationFailed;
		}

		const FActiveGameplayEffectHandle AppliedCost =
			AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*CostSpec.Data.Get());

		if (!AppliedCost.WasSuccessfullyApplied())
		{
			// Montage playback has already succeeded, so roll the new action back
			// through the same idempotent cleanup path used by cancellation.
			FinishAction(StartedAction.Value, true, 0.0f);
			return ECombatActionStartResult::RejectedCostApplicationFailed;
		}

		RestartResourceRecovery();
	}

	// Applying a GameplayEffect can synchronously invoke listeners that cancel
	// this action. Only initialize dependent melee state when it still belongs to
	// this execution. The request remains Started when the committed cost caused
	// a synchronous cancellation because both operations did occur.
	if (CurrentAction.Value == StartedAction.Value)
	{
		InitializeActionRuntime(*ActionData, MovementDirection);

	}

	OutHandle = StartedAction;

	return ECombatActionStartResult::Started;
}

void UCombatActionComponent::CancelCurrentAction(float BlendOutTime)
{
	if (!CurrentAction.IsValid())
	{
		return;
	}

	FinishAction(CurrentAction.Value, true, BlendOutTime);
}

bool UCombatActionComponent::IsActionActive() const
{
	return CurrentAction.IsValid();
}

void UCombatActionComponent::ConfigureResourceRecovery(TSubclassOf<UGameplayEffect> RecoveryEffect,
	                                                     const float DelaySeconds)
{
	StopResourceRecovery();
	ResourceRecoveryEffect = RecoveryEffect;
	ResourceRecoveryDelay = FMath::Max(DelaySeconds, 0.0f);
}

void UCombatActionComponent::StopResourceRecovery()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ResourceRecoveryTimer);
	}

	if (ActiveRecoveryEffect.IsValid())
	{
		if (UAbilitySystemComponent* AbilitySystemComponent = ResolveAbilitySystemComponent())
		{
			AbilitySystemComponent->RemoveActiveGameplayEffect(ActiveRecoveryEffect);
		}
	}

	ResourceRecoveryTimer.Invalidate();
	ActiveRecoveryEffect = FActiveGameplayEffectHandle();
}

bool UCombatActionComponent::HasPendingOrActiveResourceRecovery() const
{
	if (ActiveRecoveryEffect.IsValid())
	{
		return true;
	}

	const UWorld* World = GetWorld();
	return World && World->GetTimerManager().TimerExists(ResourceRecoveryTimer);
}

bool UCombatActionComponent::IsGameplayTagWindowActiveAt(const FGameplayTag& Tag,
	                                                      const double WorldTimeSeconds) const
{
	if (!CurrentAction.IsValid() || !Tag.IsValid() ||
	    !ConfiguredWindowTags.HasTagExact(Tag) || ActiveWindowDuration <= 0.0f)
	{
		return false;
	}

	const double WindowStart = ActionStartWorldTime + ActiveWindowStartTime;
	const double WindowEnd = WindowStart + ActiveWindowDuration;

	return WorldTimeSeconds >= WindowStart && WorldTimeSeconds < WindowEnd;
}

bool UCombatActionComponent::IsGameplayTagWindowApplied(const FGameplayTag& Tag) const
{
	return Tag.IsValid() && AppliedWindowTags.HasTagExact(Tag);
}

void UCombatActionComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
	                                       FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!CurrentAction.IsValid())
	{
		SetComponentTickEnabled(false);
		return;
	}

	const float PreviousElapsedTime = ActionElapsedTime;
	ActionElapsedTime += FMath::Max(DeltaTime, 0.0f);
	UpdateActionRuntime(PreviousElapsedTime, ActionElapsedTime);
}

UAbilitySystemComponent* UCombatActionComponent::ResolveAbilitySystemComponent() const
{
	ACharacter* Character = CachedCharacter.Get();

	if (!Character)
	{
		Character = Cast<ACharacter>(GetOwner());
	}
	IAbilitySystemInterface* AbilitySystemOwner = Cast<IAbilitySystemInterface>(Character);

	return AbilitySystemOwner ? AbilitySystemOwner->GetAbilitySystemComponent() : nullptr;
}

bool UCombatActionComponent::IsActionDataValid(const UCombatActionData& ActionData,
	                                            const FVector& MovementDirection) const
{
	const bool bHasMovement = ActionData.MovementDistance > KINDA_SMALL_NUMBER;
	const bool bHasWindow = !ActionData.WindowTags.IsEmpty();

	if (ActionData.MovementDistance < 0.0f || ActionData.MovementStartTime < 0.0f ||
	    ActionData.MovementDuration < 0.0f || ActionData.WindowStartTime < 0.0f ||
	    ActionData.WindowDuration < 0.0f)
	{
		return false;
	}

	if (bHasMovement &&
	    (ActionData.MovementDuration <= KINDA_SMALL_NUMBER || MovementDirection.SizeSquared2D() <= SMALL_NUMBER))
	{
		return false;
	}

	if (bHasWindow != (ActionData.WindowDuration > KINDA_SMALL_NUMBER))
	{
		return false;
	}

	return true;
}

void UCombatActionComponent::InitializeActionRuntime(const UCombatActionData& ActionData,
	                                                  const FVector& MovementDirection)
{
	ActiveMovementDirection = MovementDirection.GetSafeNormal2D();
	ActiveMovementDistance = ActionData.MovementDistance;
	ActiveMovementStartTime = ActionData.MovementStartTime;
	ActiveMovementDuration = ActionData.MovementDuration;
	ActiveWindowStartTime = ActionData.WindowStartTime;
	ActiveWindowDuration = ActionData.WindowDuration;
	ConfiguredWindowTags = ActionData.WindowTags;
	ActionElapsedTime = 0.0f;
	ActionStartWorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

	// Extracted root motion must not add a second displacement on top of the
	// collision-aware movement coordinated below.
	if (ActiveMovementDistance > KINDA_SMALL_NUMBER)
	{
		if (UAnimInstance* AnimInstance = ActiveAnimInstance.Get())
		{
			SavedRootMotionMode = static_cast<uint8>(AnimInstance->RootMotionMode.GetValue());
			AnimInstance->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
			bRootMotionModeOverridden = true;
		}
	}

	UpdateActionRuntime(0.0f, 0.0f);

	const bool bNeedsRuntimeTick = ActiveMovementDistance > KINDA_SMALL_NUMBER ||
	                               !ConfiguredWindowTags.IsEmpty();
	SetComponentTickEnabled(bNeedsRuntimeTick);
}

void UCombatActionComponent::UpdateActionRuntime(const float PreviousElapsedTime,
	                                              const float NewElapsedTime)
{
	const float WindowEndTime = ActiveWindowStartTime + ActiveWindowDuration;

	if (!ConfiguredWindowTags.IsEmpty())
	{
		if (NewElapsedTime >= ActiveWindowStartTime && NewElapsedTime < WindowEndTime)
		{
			ActivateWindowTags();
		}
		else if (NewElapsedTime >= WindowEndTime)
		{
			DeactivateWindowTags();
		}
	}

	if (ActiveMovementDistance <= KINDA_SMALL_NUMBER || ActiveMovementDuration <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float PreviousMovementAlpha = FMath::Clamp(
		(PreviousElapsedTime - ActiveMovementStartTime) / ActiveMovementDuration,
		0.0f,
		1.0f
	);
	const float NewMovementAlpha = FMath::Clamp(
		(NewElapsedTime - ActiveMovementStartTime) / ActiveMovementDuration,
		0.0f,
		1.0f
	);

	if (NewMovementAlpha > PreviousMovementAlpha)
	{
		BeginMovementControl();

		ACharacter* Character = CachedCharacter.Get();
		UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;

		if (Movement && CurrentAction.IsValid())
		{
			const FVector Delta = ActiveMovementDirection * ActiveMovementDistance *
			                      (NewMovementAlpha - PreviousMovementAlpha);
			FHitResult Hit;
			Movement->SafeMoveUpdatedComponent(Delta, Character->GetActorQuat(), true, Hit);
		}
	}

	if (NewMovementAlpha >= 1.0f)
	{
		EndMovementControl();
	}
}

void UCombatActionComponent::ClearActionRuntime()
{
	DeactivateWindowTags();
	EndMovementControl();

	if (bRootMotionModeOverridden)
	{
		if (UAnimInstance* AnimInstance = ActiveAnimInstance.Get())
		{
			AnimInstance->SetRootMotionMode(static_cast<ERootMotionMode::Type>(SavedRootMotionMode));
		}
	}

	bRootMotionModeOverridden = false;
	SavedRootMotionMode = 0;
	ActiveMovementDirection = FVector::ZeroVector;
	ActiveMovementDistance = 0.0f;
	ActiveMovementStartTime = 0.0f;
	ActiveMovementDuration = 0.0f;
	ActiveWindowStartTime = 0.0f;
	ActiveWindowDuration = 0.0f;
	ActionElapsedTime = 0.0f;
	ActionStartWorldTime = 0.0;
	ConfiguredWindowTags.Reset();
	AppliedWindowTags.Reset();
	SetComponentTickEnabled(false);
}

void UCombatActionComponent::BeginMovementControl()
{
	if (bMovementControlActive)
	{
		return;
	}

	ACharacter* Character = CachedCharacter.Get();
	UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;

	if (!Movement)
	{
		return;
	}

	SavedMovementMode = static_cast<uint8>(Movement->MovementMode.GetValue());
	SavedCustomMovementMode = Movement->CustomMovementMode;
	Movement->StopMovementImmediately();
	Character->ConsumeMovementInputVector();
	Movement->SetMovementMode(MOVE_None);
	bMovementControlActive = true;
}

void UCombatActionComponent::EndMovementControl()
{
	if (!bMovementControlActive)
	{
		return;
	}

	bMovementControlActive = false;

	ACharacter* Character = CachedCharacter.Get();
	UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;

	if (!Movement || Character->IsActorBeingDestroyed())
	{
		return;
	}

	Movement->StopMovementImmediately();
	Character->ConsumeMovementInputVector();
	Movement->SetMovementMode(static_cast<EMovementMode>(SavedMovementMode), SavedCustomMovementMode);
}

void UCombatActionComponent::ActivateWindowTags()
{
	if (!AppliedWindowTags.IsEmpty() || ConfiguredWindowTags.IsEmpty())
	{
		return;
	}

	if (UAbilitySystemComponent* AbilitySystemComponent = ResolveAbilitySystemComponent())
	{
		AbilitySystemComponent->AddLooseGameplayTags(ConfiguredWindowTags);
		AppliedWindowTags = ConfiguredWindowTags;
	}
}

void UCombatActionComponent::DeactivateWindowTags()
{
	if (AppliedWindowTags.IsEmpty())
	{
		return;
	}

	if (UAbilitySystemComponent* AbilitySystemComponent = ResolveAbilitySystemComponent())
	{
		AbilitySystemComponent->RemoveLooseGameplayTags(AppliedWindowTags);
	}

	AppliedWindowTags.Reset();
}

void UCombatActionComponent::RestartResourceRecovery()
{
	StopResourceRecovery();

	if (!ResourceRecoveryEffect)
	{
		return;
	}

	UWorld* World = GetWorld();

	if (!World || ResourceRecoveryDelay <= KINDA_SMALL_NUMBER)
	{
		StartResourceRecovery();
		return;
	}

	World->GetTimerManager().SetTimer(
		ResourceRecoveryTimer,
		this,
		&UCombatActionComponent::StartResourceRecovery,
		ResourceRecoveryDelay,
		false
	);
}

void UCombatActionComponent::StartResourceRecovery()
{
	ResourceRecoveryTimer.Invalidate();

	UAbilitySystemComponent* AbilitySystemComponent = ResolveAbilitySystemComponent();
	const UGameplayEffect* RecoveryEffect = ResourceRecoveryEffect
		                                      ? ResourceRecoveryEffect.GetDefaultObject()
		                                      : nullptr;

	if (!AbilitySystemComponent || !RecoveryEffect ||
	    RecoveryEffect->DurationPolicy == EGameplayEffectDurationType::Instant)
	{
		return;
	}

	FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
	Context.AddSourceObject(CachedCharacter.Get());

	const FGameplayEffectSpecHandle Spec =
		AbilitySystemComponent->MakeOutgoingSpec(ResourceRecoveryEffect, 1.0f, Context);

	if (Spec.IsValid())
	{
		ActiveRecoveryEffect = AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
}

void UCombatActionComponent::NotifyResourceCostCommitted()
{
	RestartResourceRecovery();
}

int32 UCombatActionComponent::AllocateActionInstanceId()
{
	const int32 AllocatedId = NextActionInstanceId;

	NextActionInstanceId = NextActionInstanceId == MAX_int32 ? 1 : NextActionInstanceId + 1;

	return AllocatedId;
}

void UCombatActionComponent::HandleMontageEnded(UAnimMontage*, bool, int32 ExpectedActionId)
{
	FinishAction(ExpectedActionId, false, 0.0f);
}

void UCombatActionComponent::FinishAction(int32 ExpectedActionId, bool bStopMontage, float BlendOutTime)
{
	// Only the current action may clear its state.
	// Ignore callbacks whose ID does not match the active action.
	if (!CurrentAction.IsValid() || CurrentAction.Value != ExpectedActionId)
	{
		return;
	}

	UAnimInstance* AnimInstance = ActiveAnimInstance.Get();
	UAnimMontage* AnimMontage = ActiveMontage.Get();

	// End dependent action-scoped state while its exact session handle is stable.
	// The melee component rejects stale cleanup from an older action execution.
	if (UCombatMeleeComponent* Melee = MeleeComponent.Get())
	{
		Melee->EndSession(ActiveMeleeSession);
	}

	ClearActionRuntime();

	// Clear the component's action state before calling animation functions.
	// Stopping a Montage may trigger callbacks that re-enter this component.
	ActiveMontage = nullptr;
	ActiveAnimInstance.Reset();
	ActiveMeleeSession.Reset();
	CurrentAction.Reset();

	if (!AnimInstance || !AnimMontage)
	{
		return;
	}

	// Explicit cancellation calls Montage_Stop, which may later invoke the end callback.
	// The action state has already been cleared, so remove the callback before stopping the Montage.
	FOnMontageEnded EmptyDelegate;

	AnimInstance->Montage_SetEndDelegate(EmptyDelegate, AnimMontage);

	if (bStopMontage && AnimInstance->Montage_IsActive(AnimMontage))
	{
		AnimInstance->Montage_Stop(FMath::Max(BlendOutTime, 0.0f), AnimMontage);
	}
}
