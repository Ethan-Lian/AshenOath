#include "Characters/AshenOathBossCharacter.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/AshenOathAttributeSet.h"
#include "Components/StateTreeComponent.h"
#include "Damage/CombatDamageComponent.h"
#include "Game/AshenOathGameMode.h"
#include "GameplayEffect.h"
#include "GameplayTags/AshenOathGameplayTags.h"

AAshenOathBossCharacter::AAshenOathBossCharacter()
{
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));

	AbilitySystemComponent->SetIsReplicated(false);

	AttributeSet = CreateDefaultSubobject<UAshenOathAttributeSet>(TEXT("AttributeSet"));

	StateTreeComponent = CreateDefaultSubobject<UStateTreeComponent>(TEXT("StateTreeComponent"));

	CombatDamageComponent = CreateDefaultSubobject<UCombatDamageComponent>(TEXT("CombatDamageComponent"));
	CombatDamageComponent->ConfigureInvulnerabilityTag(AshenOathGameplayTags::State_Invulnerable);

	StateTreeComponent->SetStartLogicAutomatically(false);
}

void AAshenOathBossCharacter::BeginPlay()
{
	Super::BeginPlay();

	check(AbilitySystemComponent);
	check(AttributeSet);
	check(StateTreeComponent);

	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	ApplyInitialAttributes();

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
		StateTreeComponent->StopLogic(TEXT("Boss EndPlay"));
	}

	// unregister boss so the UI can remove the corresponding boss UI.
	if (UWorld* World = GetWorld())
	{
		if (AAshenOathGameMode* GameMode = World->GetAuthGameMode<AAshenOathGameMode>())
		{
			GameMode->UnregisterBoss(this);
		}
	}

	if (AbilitySystemComponent)
	{
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
	EffectContext.AddSourceObject(this);

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
