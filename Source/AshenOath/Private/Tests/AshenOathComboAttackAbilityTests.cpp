#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/Ability/AshenOathComboAttackAbility.h"
#include "AbilitySystem/Data/AshenOathComboAttackData.h"
#include "AbilitySystem/AshenOathAttributeSet.h"
#include "Animation/AnimNotifyState_PlayerComboWindow.h"
#include "Characters/AshenOathBossCharacter.h"
#include "Characters/AshenOathPlayerCharacter.h"

#include "Abilities/GameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "Actions/CombatMeleeComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameplayAbilitySpec.h"
#include "GameplayTags/AshenOathGameplayTags.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAshenOathComboWindowConfigurationTest,
	"AshenOath.Combat.Ability.ComboWindowConfiguration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FAshenOathComboWindowConfigurationTest::RunTest(const FString& Parameters)
{
	const UAshenOathComboAttackData* ActionData = LoadObject<UAshenOathComboAttackData>(
		nullptr,
		TEXT("/Game/AshenOath/CombatData/Player/DA_Player_ComboAttack.DA_Player_ComboAttack")
	);
	TestNotNull(TEXT("Configured combo data loads"), ActionData);
	if (!ActionData || !ActionData->Montage)
	{
		return false;
	}

	UAnimMontage* Montage = ActionData->Montage;
	TestEqual(TEXT("Combo has four configured sections"), ActionData->ComboSections.Num(), 4);
	if (ActionData->ComboSections.Num() != 4)
	{
		return false;
	}

	for (int32 SectionIndex = 0; SectionIndex < 3; ++SectionIndex)
	{
		const FName SectionName = ActionData->ComboSections[SectionIndex];
		const int32 MontageSectionIndex = Montage->GetSectionIndex(SectionName);
		TestTrue(TEXT("Combo section exists in montage"), MontageSectionIndex != INDEX_NONE);
		if (MontageSectionIndex == INDEX_NONE)
		{
			continue;
		}

		float SectionStart = 0.0f;
		float SectionEnd = 0.0f;
		Montage->GetSectionStartAndEndTime(MontageSectionIndex, SectionStart, SectionEnd);
		int32 WindowCount = 0;
		for (const FAnimNotifyEvent& Event : Montage->Notifies)
		{
			if (IsValid(Event.NotifyStateClass) &&
				Event.NotifyStateClass->IsA<UAnimNotifyState_PlayerComboWindow>() &&
				Event.GetTriggerTime() >= SectionStart &&
				Event.GetTriggerTime() < SectionEnd)
			{
				++WindowCount;
				TestTrue(*FString::Printf(TEXT("%s ComboWindow ends before its section"),
					*SectionName.ToString()),
					Event.GetTriggerTime() + Event.GetDuration() < SectionEnd - 0.001f);
				AddInfo(FString::Printf(TEXT("%s ComboWindow %.3f-%.3f Section %.3f-%.3f TickType %d"),
					*SectionName.ToString(), Event.GetTriggerTime(),
					Event.GetTriggerTime() + Event.GetDuration(), SectionStart, SectionEnd,
					static_cast<int32>(Event.MontageTickType.GetValue())));
			}
		}
		TestEqual(*FString::Printf(TEXT("%s has exactly one ComboWindow"), *SectionName.ToString()),
			WindowCount, 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAshenOathComboAttackAbilityLifecycleTest,
	"AshenOath.Combat.Ability.ComboAttackLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FAshenOathComboAttackAbilityLifecycleTest::RunTest(const FString& Parameters)
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
	UClass* BossClass = LoadClass<AAshenOathBossCharacter>(
		nullptr,
		TEXT("/Game/AshenOath/Characters/BP_AshenOathBoss.BP_AshenOathBoss_C")
	);

	AAshenOathPlayerCharacter* Player = PlayerClass
		? World->SpawnActor<AAshenOathPlayerCharacter>(PlayerClass)
		: nullptr;
	AAshenOathBossCharacter* Target = BossClass
		? World->SpawnActor<AAshenOathBossCharacter>(BossClass)
		: nullptr;
	APlayerController* PlayerController = World->SpawnActor<APlayerController>();

	TestNotNull(TEXT("Playable player Blueprint loads"), Player);
	TestNotNull(TEXT("Boss Blueprint loads"), Target);
	TestNotNull(TEXT("Test controller spawns"), PlayerController);

	if (!Player || !Target || !PlayerController)
	{
		CleanupWorld();
		return false;
	}

	// The focused transient world does not route post-BeginPlay spawns through
	// the normal game loop, so dispatch the same actor lifecycle explicitly.
	Player->DispatchBeginPlay();
	Target->DispatchBeginPlay();
	PlayerController->Possess(Player);

	UAbilitySystemComponent* PlayerAbilitySystem = Player->GetAbilitySystemComponent();
	UAbilitySystemComponent* TargetAbilitySystem = Target->GetAbilitySystemComponent();
	UCombatMeleeComponent* Melee = Player->FindComponentByClass<UCombatMeleeComponent>();
	USkeletalMeshComponent* Mesh = Player->GetMesh();
	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	UCharacterMovementComponent* Movement = Player->GetCharacterMovement();

	TestNotNull(TEXT("Player ASC is ready"), PlayerAbilitySystem);
	TestNotNull(TEXT("Target ASC is ready"), TargetAbilitySystem);
	TestNotNull(TEXT("Player owns CombatMelee"), Melee);
	TestNotNull(TEXT("Player animation instance is ready"), AnimInstance);
	TestNotNull(TEXT("Player movement component is ready"), Movement);

	if (!PlayerAbilitySystem || !TargetAbilitySystem || !Melee || !Mesh || !AnimInstance || !Movement)
	{
		CleanupWorld();
		return false;
	}

	FGameplayAbilitySpec* ComboAttackSpec = nullptr;

	for (FGameplayAbilitySpec& Spec : PlayerAbilitySystem->GetActivatableAbilities())
	{
		if (Spec.Ability &&
			Spec.Ability->GetAssetTags().HasTagExact(
				AshenOathGameplayTags::Ability_Action_ComboAttack))
		{
			ComboAttackSpec = &Spec;
			break;
		}
	}

	TestNotNull(TEXT("Possession grants the configured combo attack Ability"), ComboAttackSpec);

	const UAshenOathComboAttackData* ActionData = ComboAttackSpec
		? Cast<UAshenOathComboAttackData>(ComboAttackSpec->SourceObject.Get())
		: nullptr;

	TestNotNull(TEXT("The combo attack Spec keeps ActionData as its source"), ActionData);

	if (!ComboAttackSpec || !ActionData || !ActionData->Montage ||
		ActionData->MeleeTraceBones.IsEmpty())
	{
		CleanupWorld();
		return false;
	}

	const FGameplayAttribute StaminaAttribute =
		UAshenOathAttributeSet::GetStaminaAttribute();
	const FGameplayAttribute MaxStaminaAttribute =
		UAshenOathAttributeSet::GetMaxStaminaAttribute();
	const FGameplayAttribute HealthAttribute =
		UAshenOathAttributeSet::GetHealthAttribute();

	const float StaminaBeforeAttack =
		PlayerAbilitySystem->GetNumericAttribute(StaminaAttribute);
	const float TargetHealthBeforeAttack =
		TargetAbilitySystem->GetNumericAttribute(HealthAttribute);

	Movement->Velocity = FVector(300.0f, 0.0f, 0.0f);
	Player->RequestMove(FVector2D(1.0f, 0.0f), 0.0f);
	TestFalse(TEXT("Movement input is pending before the combo attack"),
		Player->GetPendingMovementInputVector().IsNearlyZero());

	TestTrue(TEXT("Combo attack activation request is accepted"), Player->RequestComboAttack());
	TestTrue(TEXT("The combo attack Ability remains active during its Montage"), ComboAttackSpec->IsActive());
	TestTrue(TEXT("A successful combo attack owns a melee session"), Melee->HasActiveSession());
	TestTrue(TEXT("An active combo attack owns the movement-lock state"),
		PlayerAbilitySystem->HasMatchingGameplayTag(AshenOathGameplayTags::State_MovementLocked));
	TestTrue(TEXT("Starting a combo attack stops existing movement"),
		Movement->Velocity.IsNearlyZero());
	TestTrue(TEXT("Starting a combo attack clears pending movement input"),
		Player->GetPendingMovementInputVector().IsNearlyZero());

	Player->RequestMove(FVector2D(1.0f, 0.0f), 0.0f);
	TestTrue(TEXT("Movement requests are ignored during a combo attack"),
		Player->GetPendingMovementInputVector().IsNearlyZero());

	const float StaminaAfterAttack =
		PlayerAbilitySystem->GetNumericAttribute(StaminaAttribute);

	TestTrue(TEXT("A successful combo attack commits one stamina cost"),
		StaminaAfterAttack < StaminaBeforeAttack);

	const FAnimMontageInstance* FirstMontageInstance =
		AnimInstance->GetActiveInstanceForMontage(ActionData->Montage);

	TestNotNull(TEXT("GAS owns the configured combo attack Montage"), FirstMontageInstance);

	if (!FirstMontageInstance)
	{
		CleanupWorld();
		return false;
	}

	const int32 FirstMontageInstanceId = FirstMontageInstance->GetInstanceID();
	constexpr int32 FirstNotifyInstanceId = 101;

	Melee->BeginHitWindowFromAnimation(
		Mesh,
		ActionData->Montage,
		FirstMontageInstanceId,
		FirstNotifyInstanceId
	);
	TestTrue(TEXT("A matching Montage signal opens the hit window"),
		Melee->HasActiveHitWindow());

	const FVector FirstTraceLocation =
		Mesh->GetSocketLocation(ActionData->MeleeTraceBones[0]);
	Target->SetActorLocation(FirstTraceLocation, false, nullptr, ETeleportType::TeleportPhysics);
	Melee->TickHitWindowFromAnimation(
		Mesh,
		ActionData->Montage,
		FirstMontageInstanceId,
		FirstNotifyInstanceId
	);

	const float TargetHealthAfterFirstTrace =
		TargetAbilitySystem->GetNumericAttribute(HealthAttribute);
	TestTrue(TEXT("A valid melee window applies the configured damage"),
		TargetHealthAfterFirstTrace < TargetHealthBeforeAttack);

	Melee->TickHitWindowFromAnimation(
		Mesh,
		ActionData->Montage,
		FirstMontageInstanceId,
		FirstNotifyInstanceId
	);
	TestEqual(TEXT("One hit window damages the same target only once"),
		TargetAbilitySystem->GetNumericAttribute(HealthAttribute),
		TargetHealthAfterFirstTrace);

	const float StaminaBeforeRejectedRetrigger =
		PlayerAbilitySystem->GetNumericAttribute(StaminaAttribute);
	TestFalse(TEXT("An active combo attack cannot retrigger"), Player->RequestComboAttack());
	TestEqual(TEXT("A rejected retrigger does not pay a second cost"),
		PlayerAbilitySystem->GetNumericAttribute(StaminaAttribute),
		StaminaBeforeRejectedRetrigger);

	FGameplayTagContainer CombatAbilityTags;
	CombatAbilityTags.AddTag(AshenOathGameplayTags::Ability_Action);
	PlayerAbilitySystem->CancelAbilities(&CombatAbilityTags);

	TestFalse(TEXT("Cancellation ends the combo attack Ability"), ComboAttackSpec->IsActive());
	TestFalse(TEXT("Cancellation releases the melee session"), Melee->HasActiveSession());
	TestFalse(TEXT("Cancellation closes the hit window"), Melee->HasActiveHitWindow());
	TestFalse(TEXT("Cancellation releases the movement-lock state"),
		PlayerAbilitySystem->HasMatchingGameplayTag(AshenOathGameplayTags::State_MovementLocked));

	Player->RequestMove(FVector2D(1.0f, 0.0f), 0.0f);
	TestFalse(TEXT("Movement requests resume after combo-attack cancellation"),
		Player->GetPendingMovementInputVector().IsNearlyZero());
	Player->ConsumeMovementInputVector();

	AnimInstance->Montage_Stop(0.0f, ActionData->Montage);
	PlayerAbilitySystem->SetNumericAttributeBase(
		StaminaAttribute,
		PlayerAbilitySystem->GetNumericAttribute(MaxStaminaAttribute)
	);

	TestTrue(TEXT("The combo attack can activate again after cancellation"),
		Player->RequestComboAttack());

	const FAnimMontageInstance* SecondMontageInstance =
		AnimInstance->GetActiveInstanceForMontage(ActionData->Montage);

	TestNotNull(TEXT("The repeated attack owns a Montage instance"), SecondMontageInstance);

	if (!SecondMontageInstance)
	{
		CleanupWorld();
		return false;
	}

	const int32 SecondMontageInstanceId = SecondMontageInstance->GetInstanceID();
	constexpr int32 SecondNotifyInstanceId = 202;

	TestNotEqual(TEXT("Repeated playback receives a new Montage instance identity"),
		SecondMontageInstanceId,
		FirstMontageInstanceId);

	Melee->BeginHitWindowFromAnimation(
		Mesh,
		ActionData->Montage,
		FirstMontageInstanceId,
		SecondNotifyInstanceId
	);
	TestFalse(TEXT("A delayed NotifyBegin from the old Montage is ignored"),
		Melee->HasActiveHitWindow());

	Melee->BeginHitWindowFromAnimation(
		Mesh,
		ActionData->Montage,
		SecondMontageInstanceId,
		SecondNotifyInstanceId
	);
	TestTrue(TEXT("The current Montage can open its hit window"),
		Melee->HasActiveHitWindow());

	Melee->EndHitWindowFromAnimation(
		Mesh,
		ActionData->Montage,
		FirstMontageInstanceId,
		SecondNotifyInstanceId
	);
	TestTrue(TEXT("A delayed NotifyEnd cannot close the new window"),
		Melee->HasActiveHitWindow());

	Melee->EndHitWindowFromAnimation(
		Mesh,
		ActionData->Montage,
		SecondMontageInstanceId,
		SecondNotifyInstanceId
	);
	TestFalse(TEXT("The owning NotifyEnd closes the new window"),
		Melee->HasActiveHitWindow());

	PlayerAbilitySystem->CancelAbilities(&CombatAbilityTags);
	AnimInstance->Montage_Stop(0.0f, ActionData->Montage);
	PlayerAbilitySystem->SetNumericAttributeBase(StaminaAttribute, 0.0f);

	TestFalse(TEXT("Insufficient stamina rejects the activation request"),
		Player->RequestComboAttack());
	TestEqual(TEXT("Insufficient stamina is not consumed"),
		PlayerAbilitySystem->GetNumericAttribute(StaminaAttribute),
		0.0f);
	TestFalse(TEXT("Insufficient stamina creates no melee session"),
		Melee->HasActiveSession());
	TestFalse(TEXT("Insufficient stamina creates no movement lock"),
		PlayerAbilitySystem->HasMatchingGameplayTag(AshenOathGameplayTags::State_MovementLocked));

	PlayerAbilitySystem->SetNumericAttributeBase(
		StaminaAttribute,
		PlayerAbilitySystem->GetNumericAttribute(MaxStaminaAttribute)
	);

	UAshenOathComboAttackData* FailedPlaybackData =
		DuplicateObject<UAshenOathComboAttackData>(ActionData, GetTransientPackage());
	FailedPlaybackData->Montage = NewObject<UAnimMontage>(FailedPlaybackData);

	const FGameplayAbilitySpecHandle FailedPlaybackSpecHandle =
		PlayerAbilitySystem->GiveAbility(FGameplayAbilitySpec(
			ComboAttackSpec->Ability->GetClass(),
			1,
			INDEX_NONE,
			FailedPlaybackData
		));
	const float StaminaBeforeFailedPlayback =
		PlayerAbilitySystem->GetNumericAttribute(StaminaAttribute);

	// TryActivateAbility reports whether GAS accepted the request. The empty
	// Montage fails inside the task, and the Ability must roll itself back.
	PlayerAbilitySystem->TryActivateAbility(FailedPlaybackSpecHandle);

	const FGameplayAbilitySpec* FailedPlaybackSpec =
		PlayerAbilitySystem->FindAbilitySpecFromHandle(FailedPlaybackSpecHandle);
	TestTrue(TEXT("A failed Montage leaves the Ability inactive"),
		FailedPlaybackSpec && !FailedPlaybackSpec->IsActive());
	TestEqual(TEXT("A failed Montage does not commit stamina"),
		PlayerAbilitySystem->GetNumericAttribute(StaminaAttribute),
		StaminaBeforeFailedPlayback);
	TestFalse(TEXT("A failed Montage leaves no melee session"),
		Melee->HasActiveSession());
	TestFalse(TEXT("A failed Montage leaves no movement lock"),
		PlayerAbilitySystem->HasMatchingGameplayTag(AshenOathGameplayTags::State_MovementLocked));

	PlayerAbilitySystem->ClearAbility(FailedPlaybackSpecHandle);
	CleanupWorld();

	return true;
}

#endif
