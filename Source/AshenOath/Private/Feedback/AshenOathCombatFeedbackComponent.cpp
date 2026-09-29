#include "Feedback/AshenOathCombatFeedbackComponent.h"

#include "Damage/CombatDamageComponent.h"
#include "Death/CombatDeathComponent.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"

UAshenOathCombatFeedbackComponent::UAshenOathCombatFeedbackComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAshenOathCombatFeedbackComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		return;
	}

	if (UCombatDamageComponent* Damage = Owner->FindComponentByClass<UCombatDamageComponent>())
	{
		BoundDamageComponent = Damage;
		DamageResolvedHandle = Damage->OnDamageResolved().AddUObject(
			this, &UAshenOathCombatFeedbackComponent::HandleDamageResolved
		);
	}

	if (UCombatDeathComponent* Death = Owner->FindComponentByClass<UCombatDeathComponent>())
	{
		BoundDeathComponent = Death;
		DeathStartedHandle = Death->OnDeathStarted().AddUObject(
			this, &UAshenOathCombatFeedbackComponent::HandleDeathStarted
		);
	}
}

void UAshenOathCombatFeedbackComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UCombatDamageComponent* Damage = BoundDamageComponent.Get())
	{
		if (DamageResolvedHandle.IsValid())
		{
			Damage->OnDamageResolved().Remove(DamageResolvedHandle);
		}
	}
	DamageResolvedHandle.Reset();
	BoundDamageComponent.Reset();

	if (UCombatDeathComponent* Death = BoundDeathComponent.Get())
	{
		if (DeathStartedHandle.IsValid())
		{
			Death->OnDeathStarted().Remove(DeathStartedHandle);
		}
	}
	DeathStartedHandle.Reset();
	BoundDeathComponent.Reset();

	Super::EndPlay(EndPlayReason);
}

void UAshenOathCombatFeedbackComponent::HandleDamageResolved(
	const FCombatDamageAttempt& /*Attempt*/, ECombatDamageResult Result)
{
	AActor* Owner = GetOwner();

	if (!IsValid(Owner))
	{
		return;
	}

	UCombatDeathComponent* DeathComponent = BoundDeathComponent.Get();
	if (DeathComponent && DeathComponent->IsDeathStarted())
	{
		return;
	}

	USoundBase* Sound = nullptr;
	UNiagaraSystem* Effect = nullptr;

	switch (Result)
	{
	case ECombatDamageResult::Applied:
		Sound = HitSound.Get();
		Effect = HitEffect.Get();
		break;

	case ECombatDamageResult::PerfectDodge:
		Sound = PerfectDodgeSound.Get();
		Effect = PerfectDodgeEffect.Get();
		break;

	default:
		return;
	}

	FVector Location;
	FVector Extent;

	Owner->GetActorBounds(true, Location, Extent);

	if (IsValid(Sound))
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Location);
	}

	if (IsValid(Effect))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Effect, Location);
	}
}

void UAshenOathCombatFeedbackComponent::HandleDeathStarted()
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		return;
	}

	FVector Location;
	FVector Extent;

	Owner->GetActorBounds(true, Location, Extent);

	if (IsValid(DeathSound))
	{
		UGameplayStatics::PlaySoundAtLocation(this, DeathSound.Get(), Location);
	}

	if (IsValid(DeathEffect))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, DeathEffect.Get(), Location);
	}
}
