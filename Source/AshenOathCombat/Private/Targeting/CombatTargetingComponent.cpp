#include "Targeting/CombatTargetingComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"

UCombatTargetingComponent::UCombatTargetingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UCombatTargetingComponent::ConfigureTerminalStateTag(
	const FGameplayTag& InTerminalStateTag)
{
	TerminalStateTag = InTerminalStateTag;
}

bool UCombatTargetingComponent::TrySetTarget(AActor* Candidate)
{
	if (!IsTargetEligible(Candidate))
	{
		return false;
	}

	SetTarget(Candidate);
	return true;
}

void UCombatTargetingComponent::ClearTarget()
{
	SetTarget(nullptr);
}

bool UCombatTargetingComponent::HasTarget() const
{
	return CurrentTarget.IsValid();
}

AActor* UCombatTargetingComponent::GetTarget() const
{
	return CurrentTarget.Get();
}

void UCombatTargetingComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	ClearTarget();
	TargetChangedEvent.Clear();

	Super::EndPlay(EndPlayReason);
}

void UCombatTargetingComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Target = CurrentTarget.Get();
	if (Target && !IsTargetEligible(Target))
	{
		ClearTarget();
	}
}

void UCombatTargetingComponent::SetTarget(AActor* NewTarget)
{
	AActor* PreviousTarget = CurrentTarget.Get();

	if (PreviousTarget == NewTarget)
	{
		return;
	}

	if (IsValid(PreviousTarget))
	{
		PreviousTarget->OnDestroyed.RemoveDynamic(
			this,
			&UCombatTargetingComponent::HandleTargetDestroyed
		);
	}

	CurrentTarget = NewTarget;

	if (IsValid(NewTarget))
	{
		NewTarget->OnDestroyed.AddUniqueDynamic(
			this,
			&UCombatTargetingComponent::HandleTargetDestroyed
		);
	}

	TargetChangedEvent.Broadcast(PreviousTarget, NewTarget);
}

bool UCombatTargetingComponent::IsTargetEligible(
	const AActor* Candidate) const
{
	const AActor* Owner = GetOwner();

	if (!IsValid(Candidate) ||
		!IsValid(Owner) ||
		Candidate == Owner ||
		Candidate->GetWorld() != GetWorld())
	{
		return false;
	}

	const float DistanceSquared = FVector::DistSquared(
		Owner->GetActorLocation(),
		Candidate->GetActorLocation()
	);

	if (DistanceSquared > FMath::Square(MaximumTargetDistance))
	{
		return false;
	}

	if (TerminalStateTag.IsValid())
	{
		const IAbilitySystemInterface* AbilitySystemOwner =
			Cast<IAbilitySystemInterface>(Candidate);
		const UAbilitySystemComponent* TargetAbilitySystem =
			AbilitySystemOwner
				? AbilitySystemOwner->GetAbilitySystemComponent()
				: nullptr;

		if (!TargetAbilitySystem ||
			TargetAbilitySystem->HasMatchingGameplayTag(TerminalStateTag))
		{
			return false;
		}
	}

	return true;
}

void UCombatTargetingComponent::HandleTargetDestroyed(AActor* DestroyedActor)
{
	if (CurrentTarget.Get() == DestroyedActor)
	{
		ClearTarget();
	}
}
