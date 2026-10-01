#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/Ability/AshenOathComboAttackAbility.h"
#include "AbilitySystem/Ability/AshenOathDodgeAbility.h"
#include "AbilitySystem/Data/AshenOathComboAttackData.h"
#include "AbilitySystem/Data/AshenOathDodgeActionData.h"
#include "AbilitySystem/AshenOathAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Characters/AshenOathPlayerCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTags/AshenOathGameplayTags.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAshenOathComboHoldTest,
	"AshenOath.Combat.Ability.ComboTapAndHold",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FAshenOathComboHoldTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	FURL URL;
	World->InitializeActorsForPlay(URL);
	World->BeginPlay();
	auto Cleanup = [World]()
	{
		World->EndPlay(EEndPlayReason::Quit);
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	};
	UClass* PlayerClass = LoadClass<AAshenOathPlayerCharacter>(nullptr,
		TEXT("/Game/AshenOath/Characters/BP_AshenOathPlayer.BP_AshenOathPlayer_C"));
	AAshenOathPlayerCharacter* Player = PlayerClass ? World->SpawnActor<AAshenOathPlayerCharacter>(PlayerClass) : nullptr;
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	if (!TestNotNull(TEXT("Player loads"), Player) || !TestNotNull(TEXT("Controller spawns"), Controller))
	{
		Cleanup();
		return false;
	}
	Player->DispatchBeginPlay();
	Controller->Possess(Player);
	UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
	FGameplayAbilitySpec* Spec = nullptr;
	for (FGameplayAbilitySpec& Candidate : ASC->GetActivatableAbilities())
	{
		if (Candidate.Ability->IsA<UAshenOathComboAttackAbility>())
		{
			Spec = &Candidate;
			break;
		}
	}
	if (!TestNotNull(TEXT("Combo spec exists"), Spec))
	{
		Cleanup();
		return false;
	}
	const UAshenOathComboAttackData* Data = Cast<UAshenOathComboAttackData>(Spec->SourceObject.Get());
	UAshenOathComboAttackAbility* Combo = Cast<UAshenOathComboAttackAbility>(Spec->GetPrimaryInstance());
	UAnimInstance* Anim = Player->GetMesh()->GetAnimInstance();
	if (!Data || !Combo || !Anim || Data->ComboSections.Num() != 4)
	{
		AddError(TEXT("Configured four-section combo and animation must be ready"));
		Cleanup();
		return false;
	}
	UAnimMontage* Montage = Data->Montage;
	const int32 FirstSection = Montage->GetSectionIndex(Data->ComboSections[0]);
	const int32 SecondSection = Montage->GetSectionIndex(Data->ComboSections[1]);
	auto BeginWindow = [Combo, Player, Montage](int32 PlaybackId, int32 NotifyId)
	{
		Combo->BeginComboWindowFromAnimation(Player->GetMesh(), Montage, PlaybackId, NotifyId);
	};
	auto EndWindow = [Combo, Player, Montage](int32 PlaybackId, int32 NotifyId)
	{
		Combo->EndComboWindowFromAnimation(Player->GetMesh(), Montage, PlaybackId, NotifyId);
	};

	TestTrue(TEXT("Press starts a combo"), Player->RequestComboAttackPressed());
	Player->RequestComboAttackReleased();
	FAnimMontageInstance* Playback = Anim->GetActiveInstanceForMontage(Montage);
	if (!TestNotNull(TEXT("Tap owns a playback"), Playback))
	{
		Cleanup();
		return false;
	}
	int32 PlaybackId = Playback->GetInstanceID();
	BeginWindow(PlaybackId, 100);
	TestEqual(TEXT("A released tap does not automatically continue"), Playback->GetNextSectionID(FirstSection), INDEX_NONE);
	TestTrue(TEXT("A second press inside the window continues"), Player->RequestComboAttackPressed());
	TestEqual(TEXT("The press selects only the next section"), Playback->GetNextSectionID(FirstSection), SecondSection);
	TestFalse(TEXT("One window consumes input only once"), Combo->TryQueueComboInput());
	Player->RequestComboAttackReleased();
	EndWindow(PlaybackId, 100);
	ASC->CancelAbilityHandle(Spec->Handle);
	Anim->Montage_Stop(0.0f, Montage);
	ASC->SetNumericAttributeBase(UAshenOathAttributeSet::GetStaminaAttribute(),
		ASC->GetNumericAttribute(UAshenOathAttributeSet::GetMaxStaminaAttribute()));

	TestTrue(TEXT("Holding starts a fresh combo"), Player->RequestComboAttackPressed());
	Playback = Anim->GetActiveInstanceForMontage(Montage);
	if (!TestNotNull(TEXT("Hold owns a playback"), Playback))
	{
		Cleanup();
		return false;
	}
	const float StaminaAfterStart = ASC->GetNumericAttribute(UAshenOathAttributeSet::GetStaminaAttribute());
	const int32 OldPlaybackId = PlaybackId;
	PlaybackId = Playback->GetInstanceID();
	BeginWindow(OldPlaybackId, 101);
	TestEqual(TEXT("An old playback cannot consume a hold"), Playback->GetNextSectionID(FirstSection), INDEX_NONE);
	for (int32 Index = 0; Index < 2; ++Index)
	{
		Anim->Montage_JumpToSection(Data->ComboSections[Index], Montage);
		BeginWindow(PlaybackId, 200 + Index);
		TestEqual(TEXT("Holding automatically advances each matching window"),
			Playback->GetNextSectionID(Montage->GetSectionIndex(Data->ComboSections[Index])),
			Montage->GetSectionIndex(Data->ComboSections[Index + 1]));
		EndWindow(PlaybackId, 200 + Index);
	}
	Player->RequestComboAttackReleased();
	Anim->Montage_JumpToSection(Data->ComboSections[2], Montage);
	BeginWindow(PlaybackId, 202);
	TestEqual(TEXT("Release prevents automatic continuation at the next window"),
		Playback->GetNextSectionID(Montage->GetSectionIndex(Data->ComboSections[2])), INDEX_NONE);
	TestEqual(TEXT("Continuation does not commit another cost"),
		ASC->GetNumericAttribute(UAshenOathAttributeSet::GetStaminaAttribute()), StaminaAfterStart);
	EndWindow(PlaybackId, 202);
	Player->RequestComboAttackPressed();
	ASC->CancelAbilityHandle(Spec->Handle);
	Anim->Montage_Stop(0.0f, Montage);
	ASC->SetNumericAttributeBase(UAshenOathAttributeSet::GetStaminaAttribute(),
		ASC->GetNumericAttribute(UAshenOathAttributeSet::GetMaxStaminaAttribute()));
	TestTrue(TEXT("Discrete input can start after cancellation"), Player->RequestComboAttack());
	Playback = Anim->GetActiveInstanceForMontage(Montage);
	if (TestNotNull(TEXT("Discrete playback exists"), Playback))
	{
		BeginWindow(Playback->GetInstanceID(), 300);
		TestEqual(TEXT("Cancellation clears the previous held state"), Playback->GetNextSectionID(FirstSection), INDEX_NONE);
	}
	Cleanup();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAshenOathDodgeTerrainTest,
	"AshenOath.Combat.Ability.DodgeTerrain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FAshenOathDodgeTerrainTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	FURL URL;
	World->InitializeActorsForPlay(URL);
	World->BeginPlay();
	auto Cleanup = [World]()
	{
		World->EndPlay(EEndPlayReason::Quit);
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	};
	UClass* PlayerClass = LoadClass<AAshenOathPlayerCharacter>(nullptr,
		TEXT("/Game/AshenOath/Characters/BP_AshenOathPlayer.BP_AshenOathPlayer_C"));
	AAshenOathPlayerCharacter* Player = PlayerClass ? World->SpawnActor<AAshenOathPlayerCharacter>(PlayerClass) : nullptr;
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	if (!TestNotNull(TEXT("Player loads"), Player) || !TestNotNull(TEXT("Controller spawns"), Controller))
	{
		Cleanup();
		return false;
	}
	Player->DispatchBeginPlay();
	Controller->Possess(Player);
	UCharacterMovementComponent* Movement = Player->GetCharacterMovement();
	Movement->SetComponentTickEnabled(false);
	UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
	FGameplayTagContainer ActionTags;
	ActionTags.AddTag(AshenOathGameplayTags::Ability_Action);
	auto Box = [World](const FVector& Location, const FVector& Extent, const FRotator& Rotation) -> AActor*
	{
		AActor* Actor = World->SpawnActor<AActor>();
		UBoxComponent* Collision = NewObject<UBoxComponent>(Actor);
		Actor->SetRootComponent(Collision);
		Collision->SetBoxExtent(Extent);
		Collision->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Collision->CanCharacterStepUpOn = ECB_Yes;
		Collision->RegisterComponent();
		Actor->SetActorLocationAndRotation(Location, Rotation);
		return Actor;
	};
	AActor* Floor = Box(FVector(400.0f, 0.0f, -20.0f), FVector(1500.0f, 300.0f, 20.0f), FRotator::ZeroRotator);
	const float HalfHeight = Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	auto Roll = [this, World, Player, Controller, Movement, ASC, &ActionTags](const FVector& Start) -> FVector
	{
		ASC->CancelAbilities(&ActionTags);
		Player->GetMesh()->GetAnimInstance()->Montage_Stop(0.0f);
		ASC->SetNumericAttributeBase(UAshenOathAttributeSet::GetStaminaAttribute(),
			ASC->GetNumericAttribute(UAshenOathAttributeSet::GetMaxStaminaAttribute()));
		Player->SetActorLocationAndRotation(Start, FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
		Controller->SetControlRotation(FRotator::ZeroRotator);
		Movement->SetMovementMode(MOVE_Walking);
		TestTrue(TEXT("Terrain dodge activates"), Player->RequestDodge(FVector2D(0.0f, 1.0f)));
		UAshenOathDodgeAbility* Dodge = nullptr;
		const UAshenOathDodgeActionData* Data = nullptr;
		for (FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			if (Spec.IsActive() && Spec.Ability->IsA<UAshenOathDodgeAbility>())
			{
				Dodge = Cast<UAshenOathDodgeAbility>(Spec.GetPrimaryInstance());
				Data = Cast<UAshenOathDodgeActionData>(Spec.SourceObject.Get());
				break;
			}
		}
		if (!Dodge || !Data)
		{
			AddError(TEXT("Active dodge and configuration must exist"));
			return FVector::ZeroVector;
		}
		const FVector ActualStart = Player->GetActorLocation();
		float Remaining = Data->MovementStartTime + Data->MovementDuration;
		while (Remaining > KINDA_SMALL_NUMBER)
		{
			const float Step = FMath::Min(Remaining, 1.0f / 60.0f);
			World->Tick(ELevelTick::LEVELTICK_All, Step);
			Dodge->TickTasksForTesting(Step);
			Remaining -= Step;
		}
		const FVector Travel = Player->GetActorLocation() - ActualStart;
		TestTrue(TEXT("Terrain does not increase configured horizontal travel"), Travel.Size2D() <= Data->MovementDistance + 1.0f);
		ASC->CancelAbilities(&ActionTags);
		TestEqual(TEXT("Cancellation restores walking"), Movement->MovementMode.GetValue(), MOVE_Walking);
		return Travel;
	};

	const FVector Start(0.0f, 0.0f, HalfHeight + 2.15f);
	const FVector FlatTravel = Roll(Start);
	TestTrue(TEXT("Flat floor permits forward travel"), FlatTravel.X > 200.0f);
	AActor* Step = Box(FVector(160.0f, 0.0f, 10.0f), FVector(45.0f, 200.0f, 10.0f), FRotator::ZeroRotator);
	const FVector StepTravel = Roll(Start);
	TestTrue(TEXT("A 20 cm obstacle does not stop the roll"), StepTravel.X >= FlatTravel.X * 0.9f);
	TestTrue(TEXT("Roll steps back down to the floor"), FMath::Abs(StepTravel.Z) < 3.0f);
	Step->Destroy();
	AActor* Wall = Box(FVector(160.0f, 0.0f, 100.0f), FVector(15.0f, 200.0f, 100.0f), FRotator::ZeroRotator);
	const FVector WallTravel = Roll(Start);
	TestTrue(TEXT("A tall wall still blocks the roll"), WallTravel.X < 145.0f);
	Wall->Destroy();
	Floor->Destroy();
	AActor* Ramp = Box(FVector(300.0f, 0.0f, -20.0f), FVector(1500.0f, 300.0f, 20.0f), FRotator(20.0f, 0.0f, 0.0f));
	// Pitch raises the +X end. Place the capsule just above the ramp at X=0.
	const float RampStartZ = -300.0f * FMath::Tan(FMath::DegreesToRadians(20.0f)) +
		20.0f / FMath::Cos(FMath::DegreesToRadians(20.0f)) - 20.0f + HalfHeight + 8.0f;
	const FVector UphillTravel = Roll(FVector(0.0f, 0.0f, RampStartZ));
	TestTrue(TEXT("A walkable uphill ramp retains horizontal travel"), UphillTravel.X >= FlatTravel.X * 0.9f);
	TestTrue(TEXT("The roll follows the ramp upward"), UphillTravel.Z > 30.0f);
	Ramp->Destroy();
	Cleanup();
	return true;
}

#endif
