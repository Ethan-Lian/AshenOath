#include "Feedback/AshenOathCombatFeedbackComponent.h"

#include "Damage/CombatDamageComponent.h"
#include "Death/CombatDeathComponent.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"

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

	if (Result != ECombatDamageResult::Applied)
	{
		return;
	}

	FVector Location;
	FVector Extent;

	Owner->GetActorBounds(true, Location, Extent);

	if (IsValid(HitSound))
	{
		UGameplayStatics::PlaySoundAtLocation(this, HitSound.Get(), Location);
	}

	if (IsValid(HitEffect))
	{
		UGameplayStatics::SpawnEmitterAtLocation(this, HitEffect.Get(), Location);
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
		UGameplayStatics::SpawnEmitterAtLocation(this, DeathEffect.Get(), Location);
	}
}
