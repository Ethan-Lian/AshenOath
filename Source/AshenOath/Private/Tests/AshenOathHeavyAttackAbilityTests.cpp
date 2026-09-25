#if WITH_DEV_AUTOMATION_TESTS

#include "Characters/AshenOathPlayerCharacter.h"

#include "AbilitySystem/AshenOathAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Data/AshenOathHeavyAttackData.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameplayAbilitySpec.h"
#include "GameplayTags/AshenOathGameplayTags.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAshenOathHeavyAttackLifecycleTest,
	"AshenOath.Combat.Ability.HeavyAttackLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FAshenOathHeavyAttackLifecycleTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	WorldContext.SetCurrentWorld(World);

	auto CleanupWorld = [World]()
	{
		World->EndPlay(EEndPlayReason::Quit);
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	};

	FURL URL;
	World->InitializeActorsForPlay(URL);
	World->BeginPlay();

	UClass* PlayerClass = LoadClass<AAshenOathPlayerCharacter>(
		nullptr,
		TEXT("/Game/AshenOath/Characters/BP_AshenOathPlayer.BP_AshenOathPlayer_C")
	);
	AAshenOathPlayerCharacter* Player = PlayerClass
		? World->SpawnActor<AAshenOathPlayerCharacter>(PlayerClass)
		: nullptr;
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	TestNotNull(TEXT("Playable player Blueprint loads"), Player);
	TestNotNull(TEXT("Test controller spawns"), Controller);
	if (!Player || !Controller)
	{
		CleanupWorld();
		return false;
	}

	Player->DispatchBeginPlay();
	Controller->Possess(Player);
	UAbilitySystemComponent* AbilitySystem = Player->GetAbilitySystemComponent();
	TestNotNull(TEXT("Player owns an AbilitySystemComponent"), AbilitySystem);
	if (!AbilitySystem)
	{
		CleanupWorld();
		return false;
	}

	const UAshenOathHeavyAttackData* HeavyData = nullptr;
	FGameplayAbilitySpecHandle HeavySpecHandle;
	for (const FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
	{
		if (Spec.Ability && Spec.Ability->GetAssetTags().HasTagExact(
			AshenOathGameplayTags::Ability_Action_HeavyAttack))
		{
			HeavyData = Cast<UAshenOathHeavyAttackData>(Spec.SourceObject.Get());
			HeavySpecHandle = Spec.Handle;
			break;
		}
	}
	TestNotNull(TEXT("Heavy attack has its configured ActionData"), HeavyData);
	if (!HeavyData || !HeavySpecHandle.IsValid())
	{
		CleanupWorld();
		return false;
	}

	const FGameplayAttribute StaminaAttribute = UAshenOathAttributeSet::GetStaminaAttribute();
	const float StaminaBefore = AbilitySystem->GetNumericAttribute(StaminaAttribute);
	FGameplayTagContainer CombatAbilityTags;
	CombatAbilityTags.AddTag(AshenOathGameplayTags::Ability_Action);
	auto TickWorldFor = [World](float Duration)
	{
		while (Duration > KINDA_SMALL_NUMBER)
		{
			const float Step = FMath::Min(Duration, 1.0f / 60.0f);
			World->Tick(ELevelTick::LEVELTICK_All, Step);
			Duration -= Step;
		}
	};

	TestTrue(TEXT("Heavy press starts charging"), Player->RequestHeavyAttackPressed());
	TestTrue(TEXT("Charging owns the movement lock"),
		AbilitySystem->HasMatchingGameplayTag(AshenOathGameplayTags::State_MovementLocked));
	TickWorldFor(HeavyData->ChargeDrainIntervalSeconds * 2.0f + 0.02f);
	TestTrue(TEXT("Release starts the charged attack"), Player->RequestHeavyAttackReleased());
	TestTrue(TEXT("Release commits elapsed charge cost"),
		AbilitySystem->GetNumericAttribute(StaminaAttribute) < StaminaBefore);
	AbilitySystem->CancelAbilities(&CombatAbilityTags);
	TestFalse(TEXT("Cancellation releases the movement lock"),
		AbilitySystem->HasMatchingGameplayTag(AshenOathGameplayTags::State_MovementLocked));

	AbilitySystem->SetNumericAttributeBase(
		StaminaAttribute,
		AbilitySystem->GetNumericAttribute(UAshenOathAttributeSet::GetMaxStaminaAttribute())
	);
	bool bCancelledDuringCost = false;
	const FDelegateHandle StaminaChangedHandle =
		AbilitySystem->GetGameplayAttributeValueChangeDelegate(StaminaAttribute).AddLambda(
			[AbilitySystem, &CombatAbilityTags, &bCancelledDuringCost](
				const FOnAttributeChangeData& Change)
			{
				if (Change.NewValue < Change.OldValue)
				{
					bCancelledDuringCost = true;
					AbilitySystem->CancelAbilities(&CombatAbilityTags);
				}
			}
		);
	TestTrue(TEXT("A later heavy press starts a fresh charge"),
		Player->RequestHeavyAttackPressed());
	TickWorldFor(HeavyData->ChargeDrainIntervalSeconds * 2.0f + 0.02f);
	Player->RequestHeavyAttackReleased();
	AbilitySystem->GetGameplayAttributeValueChangeDelegate(StaminaAttribute).Remove(
		StaminaChangedHandle
	);
	TestTrue(TEXT("Stamina callback cancelled the heavy attack"), bCancelledDuringCost);
	TestFalse(TEXT("Synchronous cancellation leaves no active heavy attack"),
		AbilitySystem->FindAbilitySpecFromHandle(HeavySpecHandle)->IsActive());
	TestFalse(TEXT("Synchronous cancellation releases the movement lock"),
		AbilitySystem->HasMatchingGameplayTag(AshenOathGameplayTags::State_MovementLocked));

	CleanupWorld();
	return true;
}

#endif
