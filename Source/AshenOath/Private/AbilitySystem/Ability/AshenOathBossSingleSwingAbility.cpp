#include "AbilitySystem/Ability/AshenOathBossSingleSwingAbility.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Actions/CombatActionData.h"
#include "Actions/CombatMeleeComponent.h"
#include "GameplayTags/AshenOathGameplayTags.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"

UAshenOathBossSingleSwingAbility::UAshenOathBossSingleSwingAbility()
{
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(AshenOathGameplayTags::Ability_Action);
	AssetTags.AddTag(AshenOathGameplayTags::Ability_Action_BossSingleSwing);
	SetAssetTags(AssetTags);
}

bool UAshenOathBossSingleSwingAbility::CanActivateAbility(
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

	return IsActionDataReady(
		ResolveActionData(Handle, ActorInfo),
		ActorInfo
	);
}

void UAshenOathBossSingleSwingAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	const UCombatActionData* ActionData = ResolveActionData(Handle, ActorInfo);

	if (!IsActionDataReady(ActionData, ActorInfo))
	{
		EndAbility(
			Handle,
			ActorInfo,
			ActivationInfo,
			true,
			true
		);

		return;
	}

	UAbilityTask_PlayMontageAndWait* NewMontageTask =
		UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this,
			TEXT("BossSingleSwingMontage"),
			ActionData->Montage,
			ActionData->PlayRate,
			ActionData->StartSection,
			true,
			1.0f,
			0.0f,
			true
		);

	if (!NewMontageTask)
	{
		EndAbility(
			Handle,
			ActorInfo,
			ActivationInfo,
			true,
			true
		);

		return;
	}

	MontageTask = NewMontageTask;

	NewMontageTask->OnCompleted.AddDynamic(
		this,
		&UAshenOathBossSingleSwingAbility::HandleMontageCompleted
	);
	NewMontageTask->OnInterrupted.AddDynamic(
		this,
		&UAshenOathBossSingleSwingAbility::HandleMontageInterrupted
	);
	NewMontageTask->OnCancelled.AddDynamic(
		this,
		&UAshenOathBossSingleSwingAbility::HandleMontageInterrupted
	);

	NewMontageTask->ReadyForActivation();

	// Montage startup may fail and synchronously end the Ability.
	if (!IsActive())
	{
		return;
	}

	UAbilitySystemComponent* AbilitySystemComponent =
		ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;

	if (!AbilitySystemComponent ||
		!AbilitySystemComponent->IsAnimatingAbility(this) ||
		AbilitySystemComponent->GetCurrentMontage() != ActionData->Montage)
	{
		FinishAbility(true);
		return;
	}

	USkeletalMeshComponent* MeshComponent = ActorInfo ? ActorInfo->SkeletalMeshComponent.Get() : nullptr;

	UAnimInstance* AnimInstance = ActorInfo ? ActorInfo->GetAnimInstance() : nullptr;

	UCombatMeleeComponent* CombatMeleeComponent = ResolveMeleeComponent(ActorInfo);

	FAnimMontageInstance* MontageInstance = AnimInstance
	? AnimInstance->GetActiveInstanceForMontage(ActionData->Montage)
	: nullptr;

	if (!MeshComponent ||
		!AnimInstance ||
		!IsValid(CombatMeleeComponent) ||
		!MontageInstance)
	{
		FinishAbility(true);
		return;
	}

	// Establish the session before CommitAbility. Applying a cost may invoke
	// synchronous callbacks that cancel this execution.
	ActiveMeleeSession = CombatMeleeComponent->BeginSession(
		ActionData->DamageEffect,
		ActionData->bCanTriggerPerfectDodge,
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

	ActiveMeleeComponent = CombatMeleeComponent;

	ResetCostApplicationResult();

	const bool bCommitAccepted = CommitAbility(Handle, ActorInfo, ActivationInfo);

	if (!IsActive())
	{
		return;
	}

	if (!bCommitAccepted || !DidCostApplicationSucceed())
	{
		FinishAbility(true);
		return;
	}

	if (!CombatMeleeComponent->IsSessionActive(ActiveMeleeSession))
	{
		FinishAbility(true);
	}
}

void UAshenOathBossSingleSwingAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	// Clear local ownership first so synchronous Montage callbacks
	// cannot re-enter EndAbility and clean the same melee session twice.
	UCombatMeleeComponent* MeleeComponent = ActiveMeleeComponent.Get();
	const FCombatMeleeSessionHandle SessionToEnd = ActiveMeleeSession;

	ActiveMeleeSession.Reset();
	ActiveMeleeComponent.Reset();
	MontageTask = nullptr;
	ResetCostApplicationResult();

	if (MeleeComponent)
	{
		MeleeComponent->EndSession(SessionToEnd);
	}

	Super::EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		bReplicateEndAbility,
		bWasCancelled
	);
}

bool UAshenOathBossSingleSwingAbility::IsActionDataReady(
	const UCombatActionData* ActionData,
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
		!ActionData->Montage->IsValidSectionName(
			ActionData->StartSection
		))
	{
		return false;
	}

	const UCombatMeleeComponent* Melee =
		ResolveMeleeComponent(ActorInfo);

	return Melee && Melee->CanStartSession(
		ActionData->DamageEffect,
		ActionData->MeleeTraceRadius,
		ActionData->MeleeTraceBones,
		ActorInfo->SkeletalMeshComponent.Get(),
		ActorInfo->GetAnimInstance()
	);
}

UCombatMeleeComponent* UAshenOathBossSingleSwingAbility::ResolveMeleeComponent(
	const FGameplayAbilityActorInfo* ActorInfo) const
{
	AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;

	return IsValid(AvatarActor)
		? AvatarActor->FindComponentByClass<UCombatMeleeComponent>()
		: nullptr;
}

void UAshenOathBossSingleSwingAbility::HandleMontageCompleted()
{
	FinishAbility(false);
}

void UAshenOathBossSingleSwingAbility::HandleMontageInterrupted()
{
	FinishAbility(true);
}

void UAshenOathBossSingleSwingAbility::FinishAbility(const bool bWasCancelled)
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
