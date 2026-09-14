#include "AbilitySystem/Ability/AshenOathLightAttackAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Actions/CombatActionData.h"
#include "Actions/CombatMeleeComponent.h"
#include "GameplayTags/AshenOathGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Actions/CombatActionComponent.h"
#include "ActiveGameplayEffectHandle.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameplayEffect.h"

UAshenOathLightAttackAbility::UAshenOathLightAttackAbility()
{
	// One reusable instance belongs to each granted AbilitySpec. Runtime fields
	// must therefore be reset whenever the ability ends.
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	bRetriggerInstancedAbility = false;

	// The current project is authoritative single-player and has no prediction
	// contract. Keeping execution on authority avoids implying unsupported client
	// prediction for melee traces, costs, and damage.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(AshenOathGameplayTags::Ability_Action);
	AssetTags.AddTag(AshenOathGameplayTags::Ability_Action_LightAttack);
	// Asset tags classify this ability. They are not tags required on the owner.
	SetAssetTags(AssetTags);

	// An active combat action blocks every other ability in this category.
	BlockAbilitiesWithTag.AddTag(AshenOathGameplayTags::Ability_Action);
	ActivationBlockedTags.AddTag(AshenOathGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(AshenOathGameplayTags::State_Staggered);
}

bool UAshenOathLightAttackAbility::CanActivateAbility(FGameplayAbilitySpecHandle Handle,
                                                      const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
                                                      const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	return IsActionDataReady(ResolveActionData(Handle, ActorInfo), ActorInfo);
}

bool UAshenOathLightAttackAbility::CheckCost(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	const UCombatActionData* ActionData = ResolveActionData(Handle, ActorInfo);

	if (!ActionData)
	{
		return false;
	}

	// A null CostEffect deliberately describes a free action.
	if (!ActionData->CostEffect)
	{
		return true;
	}

	UAbilitySystemComponent* AbilitySystemComponent =
		ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;

	const UGameplayEffect* CostEffect =
		ActionData->CostEffect.GetDefaultObject();

	// Action costs must be one-time transactions.
	if (!AbilitySystemComponent ||
		!CostEffect ||
		CostEffect->DurationPolicy != EGameplayEffectDurationType::Instant)
	{
		return false;
	}

	const bool bCanPayCost =
		AbilitySystemComponent->CanApplyAttributeModifiers(
			CostEffect,
			GetAbilityLevel(Handle, ActorInfo),
			MakeEffectContext(Handle, ActorInfo)
		);

	if (!bCanPayCost && OptionalRelevantTags)
	{
		const FGameplayTag& CostFailureTag =
			UAbilitySystemGlobals::Get().ActivateFailCostTag;

		if (CostFailureTag.IsValid())
		{
			OptionalRelevantTags->AddTag(CostFailureTag);
		}
	}

	return bCanPayCost;
}

void UAshenOathLightAttackAbility::ApplyCost(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	bCostApplicationSucceeded = false;

	const UCombatActionData* ActionData =
		ResolveActionData(Handle, ActorInfo);

	if (!ActionData)
	{
		return;
	}

	if (!ActionData->CostEffect)
	{
		bCostApplicationSucceeded = true;
		return;
	}

	const UGameplayEffect* CostEffect =
		ActionData->CostEffect.GetDefaultObject();

	if (!CostEffect ||
		CostEffect->DurationPolicy != EGameplayEffectDurationType::Instant)
	{
		return;
	}

	const FGameplayEffectSpecHandle CostSpec =
		MakeOutgoingGameplayEffectSpec(
			Handle,
			ActorInfo,
			ActivationInfo,
			ActionData->CostEffect,
			GetAbilityLevel(Handle, ActorInfo)
		);

	if (!CostSpec.IsValid())
	{
		return;
	}

	const FActiveGameplayEffectHandle AppliedCost =
		ApplyGameplayEffectSpecToOwner(
			Handle,
			ActorInfo,
			ActivationInfo,
			CostSpec
		);

	if (!AppliedCost.WasSuccessfullyApplied())
	{
		return;
	}

	bCostApplicationSucceeded = true;

	// Temporary migration bridge. Stage C moves recovery into its own component.
	UAbilitySystemComponent* AbilitySystemComponent =
		ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;

	AActor* AvatarActor =
		ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;

	// Applying an Effect may synchronously cause death and cancel this Ability.
	// Do not restart recovery after the death handler has stopped it.
	if (!AbilitySystemComponent ||
		!IsValid(AvatarActor) ||
		AvatarActor->IsActorBeingDestroyed() ||
		AbilitySystemComponent->HasMatchingGameplayTag(
			AshenOathGameplayTags::State_Dead))
	{
		return;
	}

	if (UCombatActionComponent* LegacyActionComponent =
		AvatarActor->FindComponentByClass<UCombatActionComponent>())
	{
		LegacyActionComponent->NotifyResourceCostCommitted();
	}
}

void UAshenOathLightAttackAbility::ActivateAbility(
	FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	const UCombatActionData* ActionData = ResolveActionData(Handle, ActorInfo);

	if (!IsActionDataReady(ActionData, ActorInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAbilityTask_PlayMontageAndWait* NewMontageTask =
		UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this,
			TEXT("LightAttackMontage"),
			ActionData->Montage,
			ActionData->PlayRate,
			ActionData->StartSection,
			true
		);

	if (!NewMontageTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	MontageTask = NewMontageTask;

	NewMontageTask->OnCompleted.AddDynamic(
		this,
		&UAshenOathLightAttackAbility::HandleMontageCompleted
	);
	NewMontageTask->OnInterrupted.AddDynamic(
		this,
		&UAshenOathLightAttackAbility::HandleMontageInterrupted
	);
	NewMontageTask->OnCancelled.AddDynamic(
		this,
		&UAshenOathLightAttackAbility::HandleMontageInterrupted
	);

	NewMontageTask->ReadyForActivation();

	// Starting the task may synchronously fail and invoke OnCancelled,
	// which ends this ability before ReadyForActivation returns.
	if (!IsActive())
	{
		return;
	}

	UAbilitySystemComponent* AbilitySystemComponent =
		ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;

	// Do not commit until GAS confirms that this Ability owns the expected
	// currently playing Montage.
	if (!AbilitySystemComponent ||
		!AbilitySystemComponent->IsAnimatingAbility(this) ||
		AbilitySystemComponent->GetCurrentMontage() != ActionData->Montage)
	{
		FinishAbility(true);
		return;
	}

	USkeletalMeshComponent* MeshComponent =
		ActorInfo ? ActorInfo->SkeletalMeshComponent.Get() : nullptr;
	UAnimInstance* AnimInstance = ActorInfo ? ActorInfo->GetAnimInstance() : nullptr;
	UCombatMeleeComponent* Melee = ResolveMeleeComponent(ActorInfo);
	const FAnimMontageInstance* MontageInstance =
		AnimInstance ? AnimInstance->GetActiveInstanceForMontage(ActionData->Montage) : nullptr;

	if (!MeshComponent || !AnimInstance || !IsValid(Melee) || !MontageInstance)
	{
		FinishAbility(true);
		return;
	}

	// The session is established before CommitAbility because applying the cost
	// may synchronously cancel this execution. EndAbility then owns one cleanup
	// path for both that cancellation and every later Montage exit.
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
		FinishAbility(true);
		return;
	}

	ActiveMeleeComponent = Melee;
	bCostApplicationSucceeded = false;

	const bool bCommitAccepted =
		CommitAbility(Handle, ActorInfo, ActivationInfo);

	// Cost Effect callbacks may synchronously cancel this Ability.
	if (!IsActive())
	{
		return;
	}

	if (!bCommitAccepted || !bCostApplicationSucceeded)
	{
		FinishAbility(true);
		return;
	}

	if (!Melee->IsSessionActive(ActiveMeleeSession))
	{
		FinishAbility(true);
	}
}

void UAshenOathLightAttackAbility::EndAbility(
	FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	UCombatMeleeComponent* Melee = ActiveMeleeComponent.Get();
	const FCombatMeleeSessionHandle SessionToEnd = ActiveMeleeSession;

	// InstancedPerActor reuses this UObject on its next activation.
	// Revoke the execution's local ownership before touching external state;
	// Montage shutdown can synchronously dispatch NotifyEnd and task callbacks.
	ActiveMeleeSession.Reset();
	ActiveMeleeComponent.Reset();
	MontageTask = nullptr;
	bCostApplicationSucceeded = false;

	if (Melee)
	{
		Melee->EndSession(SessionToEnd);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

const UCombatActionData* UAshenOathLightAttackAbility::ResolveActionData(
	FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo) const
{
	return Cast<UCombatActionData>(GetSourceObject(Handle, ActorInfo));
}

bool UAshenOathLightAttackAbility::IsActionDataReady(const UCombatActionData* ActionData,
	const FGameplayAbilityActorInfo* ActorInfo) const
{
	if (!ActionData ||
		!ActionData->Montage ||
		ActionData->PlayRate <= KINDA_SMALL_NUMBER ||
		!ActorInfo ||
		!ActorInfo->GetAnimInstance() ||
		!ActorInfo->SkeletalMeshComponent.IsValid())
	{
		return false;
	}

	if (!ActionData->StartSection.IsNone() &&
		!ActionData->Montage->IsValidSectionName(ActionData->StartSection))
	{
		return false;
	}

	const UCombatMeleeComponent* Melee = ResolveMeleeComponent(ActorInfo);

	return Melee && Melee->CanStartSession(
		ActionData->DamageEffect,
		ActionData->MeleeTraceRadius,
		ActionData->MeleeTraceBones,
		ActorInfo->SkeletalMeshComponent.Get(),
		ActorInfo->GetAnimInstance()
	);
}

UCombatMeleeComponent* UAshenOathLightAttackAbility::ResolveMeleeComponent(
	const FGameplayAbilityActorInfo* ActorInfo) const
{
	AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;

	return IsValid(AvatarActor)
		? AvatarActor->FindComponentByClass<UCombatMeleeComponent>()
		: nullptr;
}

void UAshenOathLightAttackAbility::HandleMontageCompleted()
{
	FinishAbility(false);
}

void UAshenOathLightAttackAbility::HandleMontageInterrupted()
{
	FinishAbility(true);
}

void UAshenOathLightAttackAbility::FinishAbility(bool bWasCancelled)
{
	if (!IsActive())
	{
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();

	if (!ActorInfo)
	{
		return;
	}
	EndAbility(
		GetCurrentAbilitySpecHandle(),
		ActorInfo,
		GetCurrentActivationInfo(),
		true,
		bWasCancelled
	);
}
