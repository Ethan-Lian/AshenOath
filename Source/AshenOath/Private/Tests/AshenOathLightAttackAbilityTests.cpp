#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/Ability/AshenOathLightAttackAbility.h"
#include "AbilitySystem/AshenOathAttributeSet.h"
#include "Characters/AshenOathBossCharacter.h"
#include "Characters/AshenOathPlayerCharacter.h"

#include "Abilities/GameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "Actions/CombatActionData.h"
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
	FAshenOathLightAttackAbilityLifecycleTest,
	"AshenOath.Combat.Ability.LightAttackLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FAshenOathLightAttackAbilityLifecycleTest::RunTest(const FString& Parameters)
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

	FGameplayAbilitySpec* LightAttackSpec = nullptr;

	for (FGameplayAbilitySpec& Spec : PlayerAbilitySystem->GetActivatableAbilities())
	{
		if (Spec.Ability &&
			Spec.Ability->GetAssetTags().HasTagExact(
				AshenOathGameplayTags::Ability_Action_LightAttack))
		{
			LightAttackSpec = &Spec;
			break;
		}
	}

	TestNotNull(TEXT("Possession grants the configured light attack Ability"), LightAttackSpec);

	const UCombatActionData* ActionData = LightAttackSpec
		? Cast<UCombatActionData>(LightAttackSpec->SourceObject.Get())
		: nullptr;

	TestNotNull(TEXT("The light attack Spec keeps ActionData as its source"), ActionData);

	if (!LightAttackSpec || !ActionData || !ActionData->Montage ||
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
	TestFalse(TEXT("Movement input is pending before the light attack"),
		Player->GetPendingMovementInputVector().IsNearlyZero());

	TestTrue(TEXT("Light attack activation request is accepted"), Player->RequestLightAttack());
	TestTrue(TEXT("The light attack Ability remains active during its Montage"), LightAttackSpec->IsActive());
	TestTrue(TEXT("A successful light attack owns a melee session"), Melee->HasActiveSession());
	TestTrue(TEXT("An active light attack owns the movement-lock state"),
		PlayerAbilitySystem->HasMatchingGameplayTag(AshenOathGameplayTags::State_MovementLocked));
	TestTrue(TEXT("Starting a light attack stops existing movement"),
		Movement->Velocity.IsNearlyZero());
	TestTrue(TEXT("Starting a light attack clears pending movement input"),
		Player->GetPendingMovementInputVector().IsNearlyZero());

	Player->RequestMove(FVector2D(1.0f, 0.0f), 0.0f);
	TestTrue(TEXT("Movement requests are ignored during a light attack"),
		Player->GetPendingMovementInputVector().IsNearlyZero());

	const float StaminaAfterAttack =
		PlayerAbilitySystem->GetNumericAttribute(StaminaAttribute);

	TestTrue(TEXT("A successful light attack commits one stamina cost"),
		StaminaAfterAttack < StaminaBeforeAttack);

	const FAnimMontageInstance* FirstMontageInstance =
		AnimInstance->GetActiveInstanceForMontage(ActionData->Montage);

	TestNotNull(TEXT("GAS owns the configured light attack Montage"), FirstMontageInstance);

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
	TestFalse(TEXT("An active light attack cannot retrigger"), Player->RequestLightAttack());
	TestEqual(TEXT("A rejected retrigger does not pay a second cost"),
		PlayerAbilitySystem->GetNumericAttribute(StaminaAttribute),
		StaminaBeforeRejectedRetrigger);

	FGameplayTagContainer CombatAbilityTags;
	CombatAbilityTags.AddTag(AshenOathGameplayTags::Ability_Action);
	PlayerAbilitySystem->CancelAbilities(&CombatAbilityTags);

	TestFalse(TEXT("Cancellation ends the light attack Ability"), LightAttackSpec->IsActive());
	TestFalse(TEXT("Cancellation releases the melee session"), Melee->HasActiveSession());
	TestFalse(TEXT("Cancellation closes the hit window"), Melee->HasActiveHitWindow());
	TestFalse(TEXT("Cancellation releases the movement-lock state"),
		PlayerAbilitySystem->HasMatchingGameplayTag(AshenOathGameplayTags::State_MovementLocked));

	Player->RequestMove(FVector2D(1.0f, 0.0f), 0.0f);
	TestFalse(TEXT("Movement requests resume after light-attack cancellation"),
		Player->GetPendingMovementInputVector().IsNearlyZero());
	Player->ConsumeMovementInputVector();

	AnimInstance->Montage_Stop(0.0f, ActionData->Montage);
	PlayerAbilitySystem->SetNumericAttributeBase(
		StaminaAttribute,
		PlayerAbilitySystem->GetNumericAttribute(MaxStaminaAttribute)
	);

	TestTrue(TEXT("The light attack can activate again after cancellation"),
		Player->RequestLightAttack());

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
		Player->RequestLightAttack());
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

	UCombatActionData* FailedPlaybackData =
		DuplicateObject<UCombatActionData>(ActionData, GetTransientPackage());
	FailedPlaybackData->Montage = NewObject<UAnimMontage>(FailedPlaybackData);

	const FGameplayAbilitySpecHandle FailedPlaybackSpecHandle =
		PlayerAbilitySystem->GiveAbility(FGameplayAbilitySpec(
			LightAttackSpec->Ability->GetClass(),
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
