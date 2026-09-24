#if WITH_DEV_AUTOMATION_TESTS

#include "Characters/AshenOathBossCharacter.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Data/AshenOathMeleeActionData.h"
#include "Actions/CombatMeleeComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffect.h"
#include "GameplayTags/AshenOathGameplayTags.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAshenOathBossSingleSwingAbilityLifecycleTest,
	"AshenOath.Combat.Ability.BossSingleSwingLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FAshenOathBossSingleSwingAbilityLifecycleTest::RunTest(const FString& Parameters)
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

	UClass* BossClass = LoadClass<AAshenOathBossCharacter>(
		nullptr,
		TEXT("/Game/AshenOath/Characters/BP_AshenOathBoss.BP_AshenOathBoss_C")
	);
	AAshenOathBossCharacter* Boss = BossClass
		? World->SpawnActor<AAshenOathBossCharacter>(BossClass)
		: nullptr;

	TestNotNull(TEXT("Boss Blueprint loads"), Boss);
	if (!Boss)
	{
		CleanupWorld();
		return false;
	}

	UAshenOathMeleeActionData* TestActionData = NewObject<UAshenOathMeleeActionData>(Boss);
	TestActionData->Montage = LoadObject<UAnimMontage>(
		nullptr,
		TEXT("/Game/ParagonSevarog/Characters/Heroes/Sevarog/Animations/Swing1_Medium_Montage.Swing1_Medium_Montage")
	);
	TestActionData->DamageEffect = LoadClass<UGameplayEffect>(
		nullptr,
		TEXT("/Game/AshenOath/AbilitySystem/Effects/Boss/GE_Boss_SingleSwing_Damage.GE_Boss_SingleSwing_Damage_C")
	);
	TestActionData->MeleeTraceRadius = 20.0f;

	USkeletalMeshComponent* Mesh = Boss->GetMesh();
	if (Mesh &&
		Mesh->DoesSocketExist(TEXT("root_weapon_r")) &&
		Mesh->DoesSocketExist(TEXT("root_weapon_end_r")))
	{
		TestActionData->MeleeTraceBones.Add(TEXT("root_weapon_r"));
		TestActionData->MeleeTraceBones.Add(TEXT("root_weapon_end_r"));
	}

	TestNotNull(TEXT("Sevarog test Montage loads"), TestActionData->Montage.Get());
	TestNotNull(TEXT("Test damage Effect loads"), TestActionData->DamageEffect.Get());
	TestEqual(TEXT("Boss test configuration has two valid trace bones"),
		TestActionData->MeleeTraceBones.Num(), 2);

	Boss->GrantSingleSwingForTesting(TestActionData);
	Boss->DispatchBeginPlay();

	UAbilitySystemComponent* AbilitySystem = Boss->GetAbilitySystemComponent();
	UCombatMeleeComponent* Melee = Boss->FindComponentByClass<UCombatMeleeComponent>();
	TestNotNull(TEXT("Boss ASC is ready"), AbilitySystem);
	TestNotNull(TEXT("Boss owns CombatMelee"), Melee);

	if (!AbilitySystem || !Melee)
	{
		CleanupWorld();
		return false;
	}

	FGameplayAbilitySpec* SingleSwingSpec = nullptr;
	for (FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
	{
		if (Spec.Ability && Spec.Ability->GetAssetTags().HasTagExact(
			AshenOathGameplayTags::Ability_Action_BossSingleSwing))
		{
			SingleSwingSpec = &Spec;
			break;
		}
	}

	TestNotNull(TEXT("Boss grants the configured single-swing Ability"), SingleSwingSpec);
	const UAshenOathMeleeActionData* ActionData = SingleSwingSpec
		? Cast<UAshenOathMeleeActionData>(SingleSwingSpec->SourceObject.Get())
		: nullptr;
	TestNotNull(TEXT("Single-swing Spec keeps ActionData as its source"), ActionData);

	if (!SingleSwingSpec || !ActionData)
	{
		CleanupWorld();
		return false;
	}

	TestTrue(TEXT("Single-swing activation request is accepted"), Boss->RequestSingleSwing());
	TestFalse(TEXT("A second request is rejected while the swing is active"), Boss->RequestSingleSwing());
	TestTrue(TEXT("Single-swing Ability remains active during its Montage"), SingleSwingSpec->IsActive());
	TestTrue(TEXT("A successful single swing owns a melee session"), Melee->HasActiveSession());

	int32 EndNotificationCount = 0;
	bool bLastEndWasCancelled = false;
	const FDelegateHandle EndNotificationHandle = Boss->OnSingleSwingEnded().AddLambda(
		[&EndNotificationCount, &bLastEndWasCancelled](const bool bWasCancelled)
		{
			++EndNotificationCount;
			bLastEndWasCancelled = bWasCancelled;
		}
	);

	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	const FAnimMontageInstance* MontageInstance = AnimInstance
		? AnimInstance->GetActiveInstanceForMontage(ActionData->Montage)
		: nullptr;
	TestNotNull(TEXT("The active single swing owns its Montage instance"), MontageInstance);

	if (!MontageInstance)
	{
		CleanupWorld();
		return false;
	}

	constexpr int32 NotifyInstanceId = 401;
	Melee->BeginHitWindowFromAnimation(
		Mesh,
		ActionData->Montage,
		MontageInstance->GetInstanceID(),
		NotifyInstanceId
	);
	TestTrue(TEXT("A matching animation signal opens the Boss hit window"),
		Melee->HasActiveHitWindow());

	Boss->CancelSingleSwing();

	TestFalse(TEXT("Cancellation ends the single-swing Ability"),
		SingleSwingSpec->IsActive());
	TestEqual(TEXT("The Boss reports one completion for the accepted request"),
		EndNotificationCount, 1);
	TestTrue(TEXT("The completion reports cancellation"), bLastEndWasCancelled);
	TestFalse(TEXT("Cancellation releases the Boss melee session"),
		Melee->HasActiveSession());
	TestFalse(TEXT("Cancellation closes the Boss hit window"),
		Melee->HasActiveHitWindow());

	int32 RequestEndCount = 0;
	FAshenOathBossAttackRequestHandle EndedRequest;
	bool bLastRequestWasCancelled = false;
	const FDelegateHandle RequestEndHandle = Boss->OnBossAttackEnded().AddLambda(
		[&RequestEndCount, &EndedRequest, &bLastRequestWasCancelled](
			FAshenOathBossAttackRequestHandle Request, bool bWasCancelled)
		{
			++RequestEndCount;
			EndedRequest = Request;
			bLastRequestWasCancelled = bWasCancelled;
		}
	);

	const FAshenOathBossAttackStartResult FirstRequest =
		Boss->RequestFirstPhaseAttack(150.0f, 225.0f);
	TestEqual(TEXT("The first-phase request runs the available swing"),
		FirstRequest.State, EAshenOathBossAttackStartState::Running);
	TestTrue(TEXT("A running request has an identity"),
		FirstRequest.RequestHandle.IsValid());
	TestFalse(TEXT("A different request cannot cancel the active swing"),
		Boss->CancelBossAttack({FirstRequest.RequestHandle.Value + 1}));
	TestTrue(TEXT("The unmatched cancellation leaves the swing active"),
		SingleSwingSpec->IsActive());
	TestTrue(TEXT("The owner can cancel its request"),
		Boss->CancelBossAttack(FirstRequest.RequestHandle));
	TestEqual(TEXT("The owned request ends exactly once"), RequestEndCount, 1);
	TestTrue(TEXT("The end notification identifies the first request"),
		EndedRequest == FirstRequest.RequestHandle);
	TestTrue(TEXT("Explicit cancellation reports failure to the task"),
		bLastRequestWasCancelled);

	const FAshenOathBossAttackStartResult SecondRequest =
		Boss->RequestBossAttack(EAshenOathBossAttackType::SingleSwing);
	TestEqual(TEXT("The next swing receives a new running request"),
		SecondRequest.State, EAshenOathBossAttackStartState::Running);
	TestFalse(TEXT("An expired request cannot cancel the next swing"),
		Boss->CancelBossAttack(FirstRequest.RequestHandle));
	TestTrue(TEXT("The next swing remains active"), SingleSwingSpec->IsActive());
	Boss->CancelBossAttack(SecondRequest.RequestHandle);
	TestEqual(TEXT("Each owned request emits one completion"), RequestEndCount, 2);
	Boss->OnBossAttackEnded().Remove(RequestEndHandle);

	TestActionData->StartSection = TEXT("MissingSection");
	TestFalse(TEXT("An invalid Boss start section rejects activation"),
		Boss->RequestSingleSwing());
	TestEqual(TEXT("A rejected swing emits no completion"),
		EndNotificationCount, 1);
	TestFalse(TEXT("A rejected swing creates no melee session"),
		Melee->HasActiveSession());

	Boss->OnSingleSwingEnded().Remove(EndNotificationHandle);

	CleanupWorld();
	return true;
}

#endif
