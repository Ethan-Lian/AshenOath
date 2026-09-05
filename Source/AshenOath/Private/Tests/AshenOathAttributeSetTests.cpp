#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/AshenOathAttributeSet.h"

#include "AbilitySystemComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"
#include "Misc/AutomationTest.h"

namespace AshenOathAttributeSetTests
{
	// Construct a transient instant effect so the test exercises GAS's real execution
	// callback path instead of calling AttributeSet hooks directly.
	void ApplyInstantModifier(
		UAbilitySystemComponent& AbilitySystem,
		const FGameplayAttribute& Attribute,
		const EGameplayModOp::Type ModifierOperation,
		const float Magnitude
	)
	{
		UGameplayEffect* GameplayEffect = NewObject<UGameplayEffect>(GetTransientPackage());
		GameplayEffect->DurationPolicy = EGameplayEffectDurationType::Instant;

		FGameplayModifierInfo& Modifier = GameplayEffect->Modifiers.AddDefaulted_GetRef();
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = ModifierOperation;
		Modifier.ModifierMagnitude = FScalableFloat(Magnitude);

		AbilitySystem.ApplyGameplayEffectToSelf(
			GameplayEffect,
			1.0f,
			AbilitySystem.MakeEffectContext()
		);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAshenOathVitalAttributeClampingTest,
	"AshenOath.AbilitySystem.AttributeSet.VitalClamping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FAshenOathVitalAttributeClampingTest::RunTest(const FString& Parameters)
{
	// GAS ActorInfo and component registration require a real world lifecycle, even
	// though no map or editor asset is needed for this focused automation test.
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	WorldContext.SetCurrentWorld(World);

	FURL URL;
	World->InitializeActorsForPlay(URL);
	World->BeginPlay();

	AActor* OwnerActor = World->SpawnActor<AActor>();
	UAbilitySystemComponent* AbilitySystem = NewObject<UAbilitySystemComponent>(
		OwnerActor,
		TEXT("AbilitySystem")
	);
	OwnerActor->AddInstanceComponent(AbilitySystem);

	UAshenOathAttributeSet* Attributes = NewObject<UAshenOathAttributeSet>(
		OwnerActor,
		TEXT("Attributes")
	);
	AbilitySystem->AddAttributeSetSubobject(Attributes);
	AbilitySystem->RegisterComponent();
	AbilitySystem->InitAbilityActorInfo(OwnerActor, OwnerActor);

	AbilitySystem->SetNumericAttributeBase(UAshenOathAttributeSet::GetMaxHealthAttribute(), 100.0f);
	AbilitySystem->SetNumericAttributeBase(UAshenOathAttributeSet::GetHealthAttribute(), 100.0f);
	AbilitySystem->SetNumericAttributeBase(UAshenOathAttributeSet::GetMaxStaminaAttribute(), 100.0f);
	AbilitySystem->SetNumericAttributeBase(UAshenOathAttributeSet::GetStaminaAttribute(), 100.0f);

	AshenOathAttributeSetTests::ApplyInstantModifier(
		*AbilitySystem,
		UAshenOathAttributeSet::GetHealthAttribute(),
		EGameplayModOp::Additive,
		-150.0f
	);
	TestEqual(TEXT("Overkill damage clamps health to zero"), Attributes->GetHealth(), 0.0f);

	AbilitySystem->SetNumericAttributeBase(UAshenOathAttributeSet::GetHealthAttribute(), 25.0f);
	AshenOathAttributeSetTests::ApplyInstantModifier(
		*AbilitySystem,
		UAshenOathAttributeSet::GetHealthAttribute(),
		EGameplayModOp::Additive,
		200.0f
	);
	TestEqual(TEXT("Overhealing clamps health to max health"), Attributes->GetHealth(), 100.0f);

	AshenOathAttributeSetTests::ApplyInstantModifier(
		*AbilitySystem,
		UAshenOathAttributeSet::GetMaxHealthAttribute(),
		EGameplayModOp::Override,
		60.0f
	);
	TestEqual(TEXT("Max health can be reduced"), Attributes->GetMaxHealth(), 60.0f);
	TestEqual(TEXT("Health follows a reduced max health"), Attributes->GetHealth(), 60.0f);

	AshenOathAttributeSetTests::ApplyInstantModifier(
		*AbilitySystem,
		UAshenOathAttributeSet::GetStaminaAttribute(),
		EGameplayModOp::Additive,
		-125.0f
	);
	TestEqual(TEXT("Excess stamina cost clamps stamina to zero"), Attributes->GetStamina(), 0.0f);

	AbilitySystem->SetNumericAttributeBase(UAshenOathAttributeSet::GetStaminaAttribute(), 100.0f);
	AshenOathAttributeSetTests::ApplyInstantModifier(
		*AbilitySystem,
		UAshenOathAttributeSet::GetMaxStaminaAttribute(),
		EGameplayModOp::Override,
		40.0f
	);
	TestEqual(TEXT("Max stamina can be reduced"), Attributes->GetMaxStamina(), 40.0f);
	TestEqual(TEXT("Stamina follows a reduced max stamina"), Attributes->GetStamina(), 40.0f);

	World->EndPlay(EEndPlayReason::Quit);
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);

	return true;
}

#endif
