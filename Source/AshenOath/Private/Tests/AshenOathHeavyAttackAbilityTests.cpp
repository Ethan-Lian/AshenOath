#if WITH_DEV_AUTOMATION_TESTS

#include "Characters/AshenOathPlayerCharacter.h"
#include "Characters/AshenOathBossCharacter.h"

#include "AbilitySystem/AshenOathAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Data/AshenOathHeavyAttackData.h"
#include "Actions/CombatMeleeComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "CoreGlobals.h"
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

	const uint64 SavedFrameCounter = GFrameCounter;
	auto CleanupWorld = [World, SavedFrameCounter]()
	{
		World->EndPlay(EEndPlayReason::Quit);
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
		GFrameCounter = SavedFrameCounter;
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
	UCombatMeleeComponent* Melee = Player->FindComponentByClass<UCombatMeleeComponent>();
	UAnimInstance* AnimInstance = Player->GetMesh()->GetAnimInstance();
	TestNotNull(TEXT("Player owns the melee component"), Melee);
	TestNotNull(TEXT("Player has an animation instance"), AnimInstance);
	if (!Melee || !AnimInstance)
	{
		CleanupWorld();
		return false;
	}
	auto RestoreStamina = [AbilitySystem, StaminaAttribute]()
	{
		AbilitySystem->SetNumericAttributeBase(
			StaminaAttribute,
			AbilitySystem->GetNumericAttribute(UAshenOathAttributeSet::GetMaxStaminaAttribute())
		);
	};
	FGameplayTagContainer CombatAbilityTags;
	CombatAbilityTags.AddTag(AshenOathGameplayTags::Ability_Action);
	auto TickWorldFor = [World](double Duration)
	{
		while (Duration > 0.0)
		{
			// Binary-exact steps preserve the one-second boundary. Advance the
			// engine frame counter so TimerManager also ticks on each test frame.
			const float Step = static_cast<float>(FMath::Min(Duration, 1.0 / 64.0));
			World->Tick(ELevelTick::LEVELTICK_All, Step);
			++GFrameCounter;
			Duration -= Step;
		}
	};

	TestTrue(TEXT("A quick tap starts charging"), Player->RequestHeavyAttackPressed());
	TestTrue(TEXT("Charging owns the movement lock"),
		AbilitySystem->HasMatchingGameplayTag(AshenOathGameplayTags::State_MovementLocked));
	TestFalse(TEXT("An immediate release does not attack"), Player->RequestHeavyAttackReleased());
	TestFalse(TEXT("A quick tap ends the heavy ability"),
		AbilitySystem->FindAbilitySpecFromHandle(HeavySpecHandle)->IsActive());
	TestFalse(TEXT("A quick tap creates no melee session"), Melee->HasActiveSession());
	TestFalse(TEXT("A quick tap stops the charge montage"),
		AnimInstance->Montage_IsPlaying(HeavyData->Montage));
	TestFalse(TEXT("A quick tap releases the movement lock"),
		AbilitySystem->HasMatchingGameplayTag(AshenOathGameplayTags::State_MovementLocked));

	TestTrue(TEXT("A fresh press can charge after a quick tap"), Player->RequestHeavyAttackPressed());
	TickWorldFor(0.99f);
	TestFalse(TEXT("Release just before one second does not attack"), Player->RequestHeavyAttackReleased());
	TestFalse(TEXT("Early release ends the ability"),
		AbilitySystem->FindAbilitySpecFromHandle(HeavySpecHandle)->IsActive());
	TestFalse(TEXT("Early release creates no melee session"), Melee->HasActiveSession());
	TestFalse(TEXT("Early release releases the movement lock"),
		AbilitySystem->HasMatchingGameplayTag(AshenOathGameplayTags::State_MovementLocked));
	const float StaminaAfterEarlyRelease = AbilitySystem->GetNumericAttribute(StaminaAttribute);
	TestTrue(TEXT("Cancelled charging retains its elapsed stamina cost"),
		StaminaAfterEarlyRelease < StaminaBefore);
	TickWorldFor(HeavyData->ChargeDrainIntervalSeconds * 2.0f + 0.02f);
	TestEqual(TEXT("Cancelled charging stops draining stamina"),
		AbilitySystem->GetNumericAttribute(StaminaAttribute), StaminaAfterEarlyRelease);

	RestoreStamina();
	TestTrue(TEXT("Heavy press starts a full one-second charge"), Player->RequestHeavyAttackPressed());
	TickWorldFor(1.0);
	TestTrue(TEXT("Release at exactly one second starts the charged attack"), Player->RequestHeavyAttackReleased());
	TestTrue(TEXT("Valid release creates its melee session"), Melee->HasActiveSession());
	TestEqual(TEXT("Valid release jumps to the thrust section"),
		AnimInstance->Montage_GetCurrentSection(HeavyData->Montage), HeavyData->ChargedReleaseSection);
	TestTrue(TEXT("Release commits elapsed charge cost"),
		AbilitySystem->GetNumericAttribute(StaminaAttribute) < StaminaBefore);
	UClass* BossClass = LoadClass<AAshenOathBossCharacter>(nullptr,
		TEXT("/Game/AshenOath/Characters/BP_AshenOathBoss.BP_AshenOathBoss_C"));
	AAshenOathBossCharacter* Target = BossClass ? World->SpawnActor<AAshenOathBossCharacter>(BossClass) : nullptr;
	TestNotNull(TEXT("Heavy hit target Blueprint loads"), Target);
	if (!Target)
	{
		CleanupWorld();
		return false;
	}
	Target->DispatchBeginPlay();
	Target->SetActorLocation(Player->GetMesh()->GetSocketLocation(HeavyData->MeleeTraceBones[0]),
		false, nullptr, ETeleportType::TeleportPhysics);
	const int32 ReleaseMontageId = AnimInstance->GetActiveInstanceForMontage(HeavyData->Montage)->GetInstanceID();
	Melee->BeginHitWindowFromAnimation(Player->GetMesh(), HeavyData->Montage, ReleaseMontageId, 501, true);
	const float HealthBeforeHeavyHit = Target->GetAbilitySystemComponent()->GetNumericAttribute(
		UAshenOathAttributeSet::GetHealthAttribute());
	Melee->TickHitWindowFromAnimation(Player->GetMesh(), HeavyData->Montage, ReleaseMontageId, 501);
	TestTrue(TEXT("Charged heavy applies damage"), Target->GetAbilitySystemComponent()->GetNumericAttribute(
		UAshenOathAttributeSet::GetHealthAttribute()) < HealthBeforeHeavyHit);
	TestTrue(TEXT("Charged heavy still staggers the target"), Target->GetAbilitySystemComponent()->HasMatchingGameplayTag(
		AshenOathGameplayTags::State_Staggered));
	Target->Destroy();
	AbilitySystem->CancelAbilities(&CombatAbilityTags);
	TestFalse(TEXT("Cancellation releases the movement lock"),
		AbilitySystem->HasMatchingGameplayTag(AshenOathGameplayTags::State_MovementLocked));

	AbilitySystem->SetNumericAttributeBase(StaminaAttribute,
		HeavyData->MaxChargeStaminaCost / HeavyData->MaxChargeDurationSeconds * 0.25f);
	TestTrue(TEXT("Low stamina still allows charging to start"), Player->RequestHeavyAttackPressed());
	TickWorldFor(0.5f);
	TestFalse(TEXT("Stamina exhaustion before one second cancels charging"),
		AbilitySystem->FindAbilitySpecFromHandle(HeavySpecHandle)->IsActive());
	TestFalse(TEXT("Early stamina exhaustion creates no melee session"), Melee->HasActiveSession());
	TestFalse(TEXT("Early stamina exhaustion releases the movement lock"),
		AbilitySystem->HasMatchingGameplayTag(AshenOathGameplayTags::State_MovementLocked));

	RestoreStamina();
	TestTrue(TEXT("A later charge can reach its maximum duration"), Player->RequestHeavyAttackPressed());
	TickWorldFor(HeavyData->MaxChargeDurationSeconds + 0.02f);
	TestTrue(TEXT("Maximum charge still releases automatically"), Melee->HasActiveSession());
	TestEqual(TEXT("Automatic release jumps to the thrust section"),
		AnimInstance->Montage_GetCurrentSection(HeavyData->Montage), HeavyData->ChargedReleaseSection);
	AbilitySystem->CancelAbilities(&CombatAbilityTags);

	RestoreStamina();
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
