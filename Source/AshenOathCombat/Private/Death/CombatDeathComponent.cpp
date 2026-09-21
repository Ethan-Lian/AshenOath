#include "Death/CombatDeathComponent.h"

#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"

UCombatDeathComponent::UCombatDeathComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCombatDeathComponent::ConfigureDeathContract(
	const FGameplayAttribute& HealthAttribute,
	const FGameplayTag& DeadStateTag)
{
	if (!bDeathStarted)
	{
		ObservedHealthAttribute = HealthAttribute;
		TerminalStateTag = DeadStateTag;
	}
}

bool UCombatDeathComponent::Initialize(
	UAbilitySystemComponent* InAbilitySystemComponent)
{
	if (bDeathStarted ||
		!IsValid(InAbilitySystemComponent) ||
		!ObservedHealthAttribute.IsValid() ||
		!TerminalStateTag.IsValid())
	{
		return false;
	}

	if (AbilitySystemComponent.Get() == InAbilitySystemComponent &&
		HealthChangedHandle.IsValid())
	{
		return true;
	}

	UnbindHealth();

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!IsValid(Character) || Character->IsActorBeingDestroyed())
	{
		return false;
	}

	CharacterOwner = Character;

	AbilitySystemComponent = InAbilitySystemComponent;

	HealthChangedHandle = InAbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(ObservedHealthAttribute)
		.AddUObject(this, &UCombatDeathComponent::HandleHealthChanged);

	TryStartDeath();

	return HealthChangedHandle.IsValid();
}

bool UCombatDeathComponent::IsDeathStarted() const
{
	return bDeathStarted;
}

void UCombatDeathComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	UAbilitySystemComponent* AbilitySystem = AbilitySystemComponent.Get();
	const bool bRemoveOwnedTerminalTag = bOwnsTerminalStateTag;
	bOwnsTerminalStateTag = false;

	if (bRemoveOwnedTerminalTag &&
		AbilitySystem &&
		TerminalStateTag.IsValid())
	{
		AbilitySystem->RemoveLooseGameplayTag(TerminalStateTag);
	}

	UnbindHealth();
	DeathStartedEvent.Clear();
	CharacterOwner.Reset();

	Super::EndPlay(EndPlayReason);
}

void UCombatDeathComponent::HandleHealthChanged(
	const FOnAttributeChangeData& ChangeData)
{
	if (ChangeData.NewValue <= 0.0f)
	{
		TryStartDeath();
	}
}

bool UCombatDeathComponent::TryStartDeath()
{
	UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();

	if (bDeathStarted ||
		!IsValid(ASC) ||
		!ObservedHealthAttribute.IsValid() ||
		!TerminalStateTag.IsValid() ||
		ASC->GetNumericAttribute(ObservedHealthAttribute) > 0.0f)
	{
		return false;
	}

	// Claim the terminal state before adding the tag because tag delegates run
	// synchronously and may re-enter cleanup, which would tear down this
	// component's bindings.
	bDeathStarted = true;
	bOwnsTerminalStateTag = true;
	ASC->AddLooseGameplayTag(TerminalStateTag);

	if (AbilitySystemComponent.Get() != ASC)
	{
		return true;
	}

	DeathStartedEvent.Broadcast();

	// Subscribers may have already started teardown, so only play if the
	// binding survived the broadcast.
	if (AbilitySystemComponent.Get() == ASC)
	{
		PlayDeathMontage();
	}

	return true;
}

void UCombatDeathComponent::UnbindHealth()
{
	UAbilitySystemComponent* AbilitySystem = AbilitySystemComponent.Get();
	if (AbilitySystem &&
		HealthChangedHandle.IsValid() &&
		ObservedHealthAttribute.IsValid())
	{
		AbilitySystem->GetGameplayAttributeValueChangeDelegate(
			ObservedHealthAttribute)
			.Remove(HealthChangedHandle);
	}

	HealthChangedHandle.Reset();
	AbilitySystemComponent.Reset();
}

void UCombatDeathComponent::PlayDeathMontage()
{
	ACharacter* Character = CharacterOwner.Get();
	if (IsValid(Character) && IsValid(DeathMontage))
	{
		Character->PlayAnimMontage(DeathMontage);
	}
}
