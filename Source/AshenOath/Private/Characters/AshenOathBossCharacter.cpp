#include "Characters/AshenOathBossCharacter.h"

#include "AIController.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Ability/AshenOathBossSingleSwingAbility.h"
#include "AbilitySystem/AshenOathAttributeSet.h"
#include "Actions/CombatActionData.h"
#include "Actions/CombatMeleeComponent.h"
#include "Components/StateTreeComponent.h"
#include "Damage/CombatDamageComponent.h"
#include "Game/AshenOathGameMode.h"
#include "GameplayEffect.h"
#include "GameplayTags/AshenOathGameplayTags.h"

AAshenOathBossCharacter::AAshenOathBossCharacter()
{
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));

	AbilitySystemComponent->SetIsReplicated(false);

	AttributeSet = CreateDefaultSubobject<UAshenOathAttributeSet>(TEXT("AttributeSet"));

	StateTreeComponent = CreateDefaultSubobject<UStateTreeComponent>(TEXT("StateTreeComponent"));

	CombatDamageComponent = CreateDefaultSubobject<UCombatDamageComponent>(TEXT("CombatDamageComponent"));
	CombatDamageComponent->ConfigureInvulnerabilityTag(AshenOathGameplayTags::State_Invulnerable);
	CombatMeleeComponent = CreateDefaultSubobject<UCombatMeleeComponent>(TEXT("CombatMeleeComponent"));

	SingleSwingAbilityClass = UAshenOathBossSingleSwingAbility::StaticClass();

	StateTreeComponent->SetStartLogicAutomatically(false);
}

void AAshenOathBossCharacter::BeginPlay()
{
	Super::BeginPlay();

	check(AbilitySystemComponent);
	check(AttributeSet);
	check(StateTreeComponent);

	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	AbilityEndedDelegateHandle = AbilitySystemComponent->OnAbilityEnded.AddUObject(
		this,
		&AAshenOathBossCharacter::HandleAbilityEnded
	);
	ApplyInitialAttributes();

	if (!bInitialAttributesApplied)
	{
		return;
	}

	if (!GrantConfiguredAbilities())
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

	if (AbilitySystemComponent && AbilityEndedDelegateHandle.IsValid())
	{
		AbilitySystemComponent->OnAbilityEnded.Remove(AbilityEndedDelegateHandle);
		AbilityEndedDelegateHandle.Reset();
	}

	CancelCombatAbilities();
	SingleSwingAbilitySpecHandle = FGameplayAbilitySpecHandle();
	SingleSwingEndedEvent.Clear();

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

bool AAshenOathBossCharacter::RequestSingleSwing()
{
	return AbilitySystemComponent &&
		SingleSwingAbilitySpecHandle.IsValid() &&
		AbilitySystemComponent->TryActivateAbility(SingleSwingAbilitySpecHandle);
}

bool AAshenOathBossCharacter::IsSingleSwingActive() const
{
	const FGameplayAbilitySpec* Spec = AbilitySystemComponent && SingleSwingAbilitySpecHandle.IsValid()
		? AbilitySystemComponent->FindAbilitySpecFromHandle(SingleSwingAbilitySpecHandle)
		: nullptr;

	return Spec && Spec->IsActive();
}

void AAshenOathBossCharacter::CancelSingleSwing()
{
	if (AbilitySystemComponent && IsSingleSwingActive())
	{
		AbilitySystemComponent->CancelAbilityHandle(SingleSwingAbilitySpecHandle);
	}
}

#if WITH_DEV_AUTOMATION_TESTS
void AAshenOathBossCharacter::GrantSingleSwingForTesting(UCombatActionData* ActionData)
{
	if (!IsValid(ActionData) || SingleSwingAbilitySpecHandle.IsValid())
	{
		return;
	}

	SingleSwingAction = ActionData;
	GrantConfiguredAbilities();
}
#endif

bool AAshenOathBossCharacter::GrantConfiguredAbilities()
{
	if (SingleSwingAbilitySpecHandle.IsValid())
	{
		return true;
	}

	if (!AbilitySystemComponent ||
		!SingleSwingAbilityClass ||
		!SingleSwingAction)
	{
		return false;
	}

	SingleSwingAbilitySpecHandle = AbilitySystemComponent->GiveAbility(
		FGameplayAbilitySpec(
			SingleSwingAbilityClass,
			1,
			INDEX_NONE,
			SingleSwingAction
		)
	);

	return SingleSwingAbilitySpecHandle.IsValid();
}

void AAshenOathBossCharacter::CancelCombatAbilities()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	FGameplayTagContainer AbilityTags;
	AbilityTags.AddTag(AshenOathGameplayTags::Ability_Action);
	AbilitySystemComponent->CancelAbilities(&AbilityTags);
}

void AAshenOathBossCharacter::HandleAbilityEnded(const FAbilityEndedData& EndedData)
{
	if (EndedData.AbilitySpecHandle == SingleSwingAbilitySpecHandle)
	{
		SingleSwingEndedEvent.Broadcast(EndedData.bWasCancelled);
	}
}
