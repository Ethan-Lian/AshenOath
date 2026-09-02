#include "Characters/AshenOathPlayerCharacter.h"
#include "AbilitySystem/AshenOathAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"

AAshenOathPlayerCharacter::AAshenOathPlayerCharacter()
{
	AbilitySystemComponent =
		CreateDefaultSubobject<UAbilitySystemComponent>(
			TEXT("AbilitySystemComponent")
		);

	AbilitySystemComponent->SetIsReplicated(false);

	AttributeSet =
		CreateDefaultSubobject<UAshenOathAttributeSet>(
			TEXT("AttributeSet")
		);
}

void AAshenOathPlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	
	check(AbilitySystemComponent);
	check(AttributeSet);

	// Possession is the point at which the authoritative player Controller is known.
	// Reinitializing ActorInfo also refreshes GAS's cached controller/avatar references.
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	
	ApplyInitialAttributes();
}


void AAshenOathPlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AbilitySystemComponent)
	{
		// ActorInfo contains weak references into the world; release them before teardown.
		AbilitySystemComponent->ClearActorInfo();
	}
	
	Super::EndPlay(EndPlayReason);
}

UAbilitySystemComponent* AAshenOathPlayerCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

const UAshenOathAttributeSet* AAshenOathPlayerCharacter::GetAttributeSet() const
{
	return AttributeSet;
}


void AAshenOathPlayerCharacter::ApplyInitialAttributes()
{
	if (bInitialAttributesApplied || !InitialAttributesEffect)
	{
		return;
	}

	FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();

	// Record which character created this effect. Calculations can read this source later.
	EffectContext.AddSourceObject(this);

	// The GameplayEffect class is only a template. MakeOutgoingSpec creates the
	// runtime effect data GAS can apply, including its level and context.
	const FGameplayEffectSpecHandle EffectSpec =
		AbilitySystemComponent->MakeOutgoingSpec(
			InitialAttributesEffect,
			1.0f,
			EffectContext
		);

	if (!EffectSpec.IsValid())
	{
		return;
	}

	const FActiveGameplayEffectHandle AppliedHandle =
		AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(
			*EffectSpec.Data.Get()
		);

	if (AppliedHandle.WasSuccessfullyApplied())
	{
		bInitialAttributesApplied = true;
	}
}





