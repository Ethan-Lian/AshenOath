#include "Characters/AshenOathBossCharacter.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/AshenOathAttributeSet.h"
#include "GameplayEffect.h"


AAshenOathBossCharacter::AAshenOathBossCharacter()
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

void AAshenOathBossCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	check(AbilitySystemComponent);
	check(AttributeSet);

	// AI has no controller-dependent initialization, so BeginPlay is its GAS boundary.
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	ApplyInitialAttributes();
}

void AAshenOathBossCharacter::ApplyInitialAttributes()
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
		AbilitySystemComponent->MakeOutgoingSpec(InitialAttributesEffect, 1.0f, EffectContext);

	if (!EffectSpec.IsValid())
	{
		return;
	}

	const FActiveGameplayEffectHandle AppliedHandle =
		AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*EffectSpec.Data.Get());

	if (AppliedHandle.WasSuccessfullyApplied())
	{
		bInitialAttributesApplied = true;
	}
}

void AAshenOathBossCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AbilitySystemComponent)
	{
		// Drop ActorInfo's world references before the actor and its components disappear.
		AbilitySystemComponent->ClearActorInfo();
	}
	
	Super::EndPlay(EndPlayReason);
}

UAbilitySystemComponent* AAshenOathBossCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

const UAshenOathAttributeSet* AAshenOathBossCharacter::GetAttributeSet() const
{
	return AttributeSet;
}
