#include "AbilitySystem/Ability/AshenOathLightAttackAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Actions/CombatActionData.h"
#include "Actions/CombatMeleeComponent.h"
#include "GameplayTags/AshenOathGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"

UAshenOathLightAttackAbility::UAshenOathLightAttackAbility()
{
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(AshenOathGameplayTags::Ability_Action);
	AssetTags.AddTag(AshenOathGameplayTags::Ability_Action_LightAttack);

	// Asset tags classify this ability. They are not tags required on the owner.
	SetAssetTags(AssetTags);

	// The Character enforces this state by clearing existing motion and rejecting
	// movement requests. GAS removes the tag on every Ability exit path.
	ActivationOwnedTags.AddTag(AshenOathGameplayTags::State_MovementLocked);
}

bool UAshenOathLightAttackAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(
		Handle,
		ActorInfo,
		SourceTags,
		TargetTags,
		OptionalRelevantTags))
	{
		return false;
	}

	return IsActionDataReady(ResolveActionData(Handle, ActorInfo), ActorInfo);
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
			true,  // Stop the montage when the Ability ends.
			1.0f,  // Keep the authored root-motion scale.
			0.0f,  // Start from the beginning of the selected section.
			true   // Report interruptions that arrive after blend-out starts.
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
	ResetCostApplicationResult();

	const bool bCommitAccepted =
		CommitAbility(Handle, ActorInfo, ActivationInfo);

	// Cost Effect callbacks may synchronously cancel this Ability.
	if (!IsActive())
	{
		return;
	}

	if (!bCommitAccepted || !DidCostApplicationSucceed())
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
	ResetCostApplicationResult();

	if (Melee)
	{
		Melee->EndSession(SessionToEnd);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
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
