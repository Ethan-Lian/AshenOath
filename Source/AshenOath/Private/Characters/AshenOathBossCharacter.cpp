#include "Characters/AshenOathBossCharacter.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/AshenOathAttributeSet.h"
#include "Components/StateTreeComponent.h"
#include "Damage/CombatDamageComponent.h"
#include "Game/AshenOathGameMode.h"
#include "GameplayEffect.h"

AAshenOathBossCharacter::AAshenOathBossCharacter()
{
	// Constructor-created subobjects are owned for the character's entire lifetime;
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));

	AbilitySystemComponent->SetIsReplicated(false);

	AttributeSet = CreateDefaultSubobject<UAshenOathAttributeSet>(TEXT("AttributeSet"));

	StateTreeComponent = CreateDefaultSubobject<UStateTreeComponent>(TEXT("StateTreeComponent"));

	CombatDamageComponent = CreateDefaultSubobject<UCombatDamageComponent>(TEXT("CombatDamageComponent"));

	StateTreeComponent->SetStartLogicAutomatically(false);
}

void AAshenOathBossCharacter::BeginPlay()
{
	Super::BeginPlay();

	check(AbilitySystemComponent);
	check(AttributeSet);
	check(StateTreeComponent);

	// AI has no controller-dependent initialization, so BeginPlay is its GAS boundary.
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	ApplyInitialAttributes();

	// A boss without initialized attributes cannot enter the combat lifecycle.
	if (!bInitialAttributesApplied)
	{
		return;
	}

	UWorld* World = GetWorld();

	AAshenOathGameMode* GameMode = World ? World->GetAuthGameMode<AAshenOathGameMode>() : nullptr;

	// Register the boss so GameMode can notify the UI system to build the corresponding boss UI.
	if (!IsValid(GameMode) || !GameMode->RegisterBoss(this))
	{
		return;
	}
	StateTreeComponent->StartLogic();
}

void AAshenOathBossCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{

	if (StateTreeComponent)
	{
		// ExitState may still need the ASC, so stop the tree first.
		StateTreeComponent->StopLogic(TEXT("Boss EndPlay"));
	}

	// When the boss leaves the gameplay lifecycle, 
	// unregister it so the UI can remove the corresponding boss UI.
	if (UWorld* World = GetWorld())
	{
		if (AAshenOathGameMode* GameMode = World->GetAuthGameMode<AAshenOathGameMode>())
		{
			GameMode->UnregisterBoss(this);
		}
	}

	if (AbilitySystemComponent)
	{
		// Drop ActorInfo's world references before the actor and its components disappear.
		AbilitySystemComponent->ClearActorInfo();
	}

	Super::EndPlay(EndPlayReason);
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

UAbilitySystemComponent* AAshenOathBossCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

const UAshenOathAttributeSet* AAshenOathBossCharacter::GetAttributeSet() const
{
	return AttributeSet;
}
