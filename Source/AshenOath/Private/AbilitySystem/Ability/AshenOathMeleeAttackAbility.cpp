#include "AbilitySystem/Ability/AshenOathMeleeAttackAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Data/AshenOathMeleeActionData.h"
#include "Actions/CombatMeleeComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"

bool UAshenOathMeleeAttackAbility::CanActivateAbility(
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

	return IsMeleeActionDataReady(
		Cast<UAshenOathMeleeActionData>(ResolveActionData(Handle, ActorInfo)),
		ActorInfo);
}

bool UAshenOathMeleeAttackAbility::StartAttackMontage(
	const FGameplayAbilityActorInfo* ActorInfo,
	const UAshenOathMeleeActionData* ActionData,
	const FName InitialSection)
{
	if (!IsActive() || MontageTask ||
		!IsMeleeActionDataReady(ActionData, ActorInfo) ||
		(!InitialSection.IsNone() &&
			!ActionData->Montage->IsValidSectionName(InitialSection)))
	{
		FinishAbility(true);
		return false;
	}

	UAbilityTask_PlayMontageAndWait* NewMontageTask =
		UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this,
			TEXT("MeleeAttackMontage"),
			ActionData->Montage,
			ActionData->PlayRate,
			InitialSection,
			true,
			1.0f,
			0.0f,
			true
		);

	if (!NewMontageTask)
	{
		FinishAbility(true);
		return false;
	}

	MontageTask = NewMontageTask;
	NewMontageTask->OnCompleted.AddDynamic(
		this,
		&UAshenOathMeleeAttackAbility::HandleMontageCompleted
	);
	NewMontageTask->OnInterrupted.AddDynamic(
		this,
		&UAshenOathMeleeAttackAbility::HandleMontageInterrupted
	);
	NewMontageTask->OnCancelled.AddDynamic(
		this,
		&UAshenOathMeleeAttackAbility::HandleMontageInterrupted
	);
	NewMontageTask->ReadyForActivation();

	if (!IsActive())
	{
		return false;
	}

	UAbilitySystemComponent* AbilitySystemComponent =
		ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;

	if (!AbilitySystemComponent ||
		!AbilitySystemComponent->IsAnimatingAbility(this) ||
		AbilitySystemComponent->GetCurrentMontage() != ActionData->Montage)
	{
		FinishAbility(true);
		return false;
	}

	USkeletalMeshComponent* MeshComponent =
		ActorInfo ? ActorInfo->SkeletalMeshComponent.Get() : nullptr;
	UAnimInstance* AnimInstance = ActorInfo ? ActorInfo->GetAnimInstance() : nullptr;
	UCombatMeleeComponent* Melee = ResolveMeleeComponent(ActorInfo);
	const FAnimMontageInstance* MontageInstance = AnimInstance
		? AnimInstance->GetActiveInstanceForMontage(ActionData->Montage)
		: nullptr;

	if (!MeshComponent || !AnimInstance || !IsValid(Melee) || !MontageInstance)
	{
		FinishAbility(true);
		return false;
	}

	ActiveSourceMesh = MeshComponent;
	ActiveSourceAnimInstance = AnimInstance;
	ActiveSourceMontage = ActionData->Montage;
	ActiveMontageInstanceId = MontageInstance->GetInstanceID();
	return true;
}

bool UAshenOathMeleeAttackAbility::BeginMeleeExecution(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const UAshenOathMeleeActionData* ActionData,
	const EMeleeCostCommit CostCommit,
	const FGameplayTag SetByCallerMagnitudeTag,
	const float SetByCallerMagnitude)
{
	USkeletalMeshComponent* MeshComponent = ActiveSourceMesh.Get();
	UAnimInstance* AnimInstance = ActiveSourceAnimInstance.Get();
	UAnimMontage* Montage = ActiveSourceMontage.Get();
	UCombatMeleeComponent* Melee = ResolveMeleeComponent(ActorInfo);
	const FAnimMontageInstance* MontageInstance = AnimInstance && Montage
		? AnimInstance->GetMontageInstanceForID(ActiveMontageInstanceId)
		: nullptr;

	if (!IsActive() || ActiveMeleeSession.IsValid() || !ActionData ||
		ActionData->Montage != Montage || !MeshComponent || !AnimInstance ||
		!IsValid(Melee) || !MontageInstance ||
		MontageInstance->Montage != Montage || !MontageInstance->IsActive())
	{
		FinishAbility(true);
		return false;
	}

	FCombatMeleeSessionRequest SessionRequest;
	SessionRequest.DamageEffect = ActionData->DamageEffect;
	SessionRequest.bCanTriggerPerfectDodge = ActionData->bCanTriggerPerfectDodge;
	SessionRequest.TraceRadius = ActionData->MeleeTraceRadius;
	SessionRequest.TraceBones = ActionData->MeleeTraceBones;
	SessionRequest.SourceMesh = MeshComponent;
	SessionRequest.SourceAnimInstance = AnimInstance;
	SessionRequest.SourceMontage = Montage;
	SessionRequest.MontageInstanceId = ActiveMontageInstanceId;
	SessionRequest.SetByCallerMagnitudeTag = SetByCallerMagnitudeTag;
	SessionRequest.SetByCallerMagnitude = SetByCallerMagnitude;
	ActiveMeleeSession = Melee->BeginSession(SessionRequest);

	if (!ActiveMeleeSession.IsValid())
	{
		FinishAbility(true);
		return false;
	}

	ActiveMeleeComponent = Melee;
	if (CostCommit == EMeleeCostCommit::SkipConfigured)
	{
		return true;
	}

	ResetCostApplicationResult();
	const bool bCommitAccepted = CommitAbility(Handle, ActorInfo, ActivationInfo);

	if (!IsActive())
	{
		return false;
	}

	if (!bCommitAccepted || !DidCostApplicationSucceed() ||
		!Melee->IsSessionActive(ActiveMeleeSession))
	{
		FinishAbility(true);
		return false;
	}

	return true;
}

void UAshenOathMeleeAttackAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	UCombatMeleeComponent* Melee = ActiveMeleeComponent.Get();
	const FCombatMeleeSessionHandle SessionToEnd = ActiveMeleeSession;

	ActiveMeleeSession.Reset();
	ActiveMeleeComponent.Reset();
	MontageTask = nullptr;
	ResetMeleeExecutionState();
	ResetCostApplicationResult();

	if (Melee)
	{
		Melee->EndSession(SessionToEnd);
	}

	Super::EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		bReplicateEndAbility,
		bWasCancelled
	);
}

bool UAshenOathMeleeAttackAbility::IsMeleeActionDataReady(
	const UAshenOathMeleeActionData* ActionData,
	const FGameplayAbilityActorInfo* ActorInfo) const
{
	if (!ActionData || !ActionData->Montage ||
		ActionData->PlayRate <= KINDA_SMALL_NUMBER || !ActorInfo ||
		!ActorInfo->GetAnimInstance() ||
		!ActorInfo->SkeletalMeshComponent.IsValid())
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

bool UAshenOathMeleeAttackAbility::IsAttackSignalOwned(
	const USkeletalMeshComponent* MeshComponent,
	const UAnimSequenceBase* Animation,
	const int32 MontageInstanceId) const
{
	const UAnimInstance* AnimInstance = ActiveSourceAnimInstance.Get();
	const UAnimMontage* Montage = ActiveSourceMontage.Get();
	const FAnimMontageInstance* MontageInstance = AnimInstance && Montage
		? AnimInstance->GetActiveInstanceForMontage(Montage)
		: nullptr;

	return IsActive() && MeshComponent &&
		MeshComponent == ActiveSourceMesh.Get() &&
		MeshComponent->GetAnimInstance() == AnimInstance && Animation &&
		Animation == Montage && MontageInstanceId != INDEX_NONE &&
		MontageInstanceId == ActiveMontageInstanceId && MontageInstance &&
		MontageInstance->GetInstanceID() == ActiveMontageInstanceId;
}

UCombatMeleeComponent* UAshenOathMeleeAttackAbility::ResolveMeleeComponent(
	const FGameplayAbilityActorInfo* ActorInfo) const
{
	AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	return IsValid(AvatarActor)
		? AvatarActor->FindComponentByClass<UCombatMeleeComponent>()
		: nullptr;
}

UAnimInstance* UAshenOathMeleeAttackAbility::GetActiveAttackAnimInstance() const
{
	return ActiveSourceAnimInstance.Get();
}

UAnimMontage* UAshenOathMeleeAttackAbility::GetActiveAttackMontage() const
{
	return ActiveSourceMontage.Get();
}

int32 UAshenOathMeleeAttackAbility::GetActiveAttackMontageInstanceId() const
{
	return ActiveMontageInstanceId;
}

void UAshenOathMeleeAttackAbility::HandleMontageCompleted()
{
	FinishAbility(false);
}

void UAshenOathMeleeAttackAbility::HandleMontageInterrupted()
{
	FinishAbility(true);
}

void UAshenOathMeleeAttackAbility::FinishAbility(const bool bWasCancelled)
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

void UAshenOathMeleeAttackAbility::ResetMeleeExecutionState()
{
	ActiveSourceMesh.Reset();
	ActiveSourceAnimInstance.Reset();
	ActiveSourceMontage.Reset();
	ActiveMontageInstanceId = INDEX_NONE;
}
