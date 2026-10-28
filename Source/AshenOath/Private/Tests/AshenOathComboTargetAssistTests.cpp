#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/Ability/AshenOathComboAttackAbility.h"
#include "AbilitySystem/AshenOathAttributeSet.h"
#include "AbilitySystem/Data/AshenOathComboAttackData.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifyState_CombatHitWindow.h"
#include "Characters/AshenOathBossCharacter.h"
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
#include "Misc/ScopeExit.h"
#include "Targeting/CombatTargetingComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAshenOathComboTargetAssistTest,
	"AshenOath.Combat.Ability.ComboTargetAssist",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FAshenOathComboTargetAssistTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	FURL URL;
	World->InitializeActorsForPlay(URL);
	World->BeginPlay();
	ON_SCOPE_EXIT
	{
		World->EndPlay(EEndPlayReason::Quit);
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	};

	UClass* PlayerClass = LoadClass<AAshenOathPlayerCharacter>(nullptr,
		TEXT("/Game/AshenOath/Characters/BP_AshenOathPlayer.BP_AshenOathPlayer_C"));
	UClass* BossClass = LoadClass<AAshenOathBossCharacter>(nullptr,
		TEXT("/Game/AshenOath/Characters/BP_AshenOathBoss.BP_AshenOathBoss_C"));
	if (!TestNotNull(TEXT("Player Blueprint loads"), PlayerClass) || !TestNotNull(TEXT("Boss Blueprint loads"), BossClass))
	{
		return false;
	}
	AAshenOathPlayerCharacter* Player = World->SpawnActor<AAshenOathPlayerCharacter>(PlayerClass);
	AAshenOathBossCharacter* Target = World->SpawnActor<AAshenOathBossCharacter>(BossClass, FVector(400.0f, 0.0f, 0.0f),
		FRotator::ZeroRotator);
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	if (!Player || !Target || !Controller)
	{
		AddError(TEXT("Test actors must spawn"));
		return false;
	}
	Player->DispatchBeginPlay();
	Target->DispatchBeginPlay();
	Controller->Possess(Player);
	Player->SetActorTickEnabled(false);
	Target->SetActorTickEnabled(false);
	Player->GetMesh()->SetComponentTickEnabled(false);
	Target->GetMesh()->SetComponentTickEnabled(false);
	Target->GetCharacterMovement()->SetComponentTickEnabled(false);
	UCharacterMovementComponent* Movement = Player->GetCharacterMovement();
	Movement->SetComponentTickEnabled(false);
	UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
	UAnimInstance* Anim = Player->GetMesh()->GetAnimInstance();
	UCombatTargetingComponent* Targeting = Player->FindComponentByClass<UCombatTargetingComponent>();
	if (!ASC || !Anim || !Targeting)
	{
		AddError(TEXT("Player combat components must initialize"));
		return false;
	}
	FGameplayAbilitySpec* ComboSpec = nullptr;
	for (FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.Ability->IsA<UAshenOathComboAttackAbility>())
		{
			ComboSpec = &Spec;
			break;
		}
	}
	const UAshenOathComboAttackData* Data = ComboSpec ? Cast<UAshenOathComboAttackData>(ComboSpec->SourceObject.Get()) : nullptr;
	if (!Data || !Data->Montage || Data->ComboSections.Num() != 4)
	{
		AddError(TEXT("The configured four-section combo must load"));
		return false;
	}
	UAnimMontage* Montage = Data->Montage;
	const ERootMotionMode::Type OriginalRootMotionMode = Anim->RootMotionMode;
	FGameplayTagContainer ActionTags;
	ActionTags.AddTag(AshenOathGameplayTags::Ability_Action);
	const float Height = Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.15f;

	auto Box = [World](const FVector& Location, const FVector& Extent)
	{
		AActor* Actor = World->SpawnActor<AActor>();
		UBoxComponent* Collision = NewObject<UBoxComponent>(Actor);
		Actor->SetRootComponent(Collision);
		Collision->SetBoxExtent(Extent);
		Collision->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Collision->RegisterComponent();
		Actor->SetActorLocation(Location);
		return Actor;
	};
	Box(FVector(0.0f, 0.0f, -20.0f), FVector(2000.0f, 2000.0f, 20.0f));
	auto Reset = [&]()
	{
		ASC->CancelAbilities(&ActionTags);
		Anim->Montage_Stop(0.0f);
		Targeting->ClearTarget();
		ASC->SetNumericAttributeBase(UAshenOathAttributeSet::GetStaminaAttribute(),
			ASC->GetNumericAttribute(UAshenOathAttributeSet::GetMaxStaminaAttribute()));
		Player->SetActorLocationAndRotation(FVector(0.0f, 0.0f, Height), FRotator(0.0f, 30.0f, 0.0f),
			false, nullptr, ETeleportType::TeleportPhysics);
		Target->SetActorLocation(FVector(400.0f, 0.0f, Height), false, nullptr, ETeleportType::TeleportPhysics);
		Movement->SetMovementMode(MOVE_Walking);
	};
	auto Tick = [World, ASC](const float Duration)
	{
		float Remaining = Duration;
		while (Remaining > KINDA_SMALL_NUMBER)
		{
			const float Step = FMath::Min(Remaining, 1.0f / 120.0f);
			World->Tick(LEVELTICK_All, Step);
			// The synchronous test world has no engine-frame progression for component ticks.
			ASC->UGameplayTasksComponent::TickComponent(Step, LEVELTICK_All, nullptr);
			Remaining -= Step;
		}
	};
	auto Start = [&]()
	{
		TestTrue(TEXT("Target can be locked"), Targeting->TrySetTarget(Target));
		TestTrue(TEXT("Combo activates"), Player->RequestComboAttack());
	};
	auto FirstHitTime = [this, Montage, Data](const int32 ComboIndex)
	{
		float StartTime = 0.0f;
		float EndTime = 0.0f;
		Montage->GetSectionStartAndEndTime(Montage->GetSectionIndex(Data->ComboSections[ComboIndex]), StartTime, EndTime);
		float HitTime = EndTime;
		for (const FAnimNotifyEvent& Event : Montage->Notifies)
		{
			if (Event.NotifyStateClass && Event.NotifyStateClass->IsA<UAnimNotifyState_CombatHitWindow>() &&
				Event.GetTriggerTime() >= StartTime && Event.GetTriggerTime() < EndTime)
			{
				HitTime = FMath::Min(HitTime, Event.GetTriggerTime());
			}
		}
		TestTrue(TEXT("Assisted section has a nonzero windup and a hit window"), HitTime > StartTime && HitTime < EndTime);
		AddInfo(FString::Printf(TEXT("%s windup %.3f-%.3f"), *Data->ComboSections[ComboIndex].ToString(), StartTime, HitTime));
		return HitTime;
	};
	const float FirstHit = FirstHitTime(0);
	const float FinisherHit = FirstHitTime(3);

	Reset();
	Start();
	TestTrue(TEXT("First hit faces the target immediately"), FMath::Abs(Player->GetActorRotation().Yaw) < 0.1f);
	Tick(Data->ApproachDuration + 0.02f);
	const float Travel = Player->GetActorLocation().X;
	TestTrue(TEXT("First hit closes the post-dodge gap"), Travel > 100.0f);
	TestTrue(TEXT("Approach respects its travel cap"), Travel <= Data->ApproachMaxTravelDistance + 1.0f);
	const float StopDistance = Player->GetCapsuleComponent()->GetScaledCapsuleRadius() +
		Target->GetCapsuleComponent()->GetScaledCapsuleRadius() + Data->ApproachStopGap;
	TestTrue(TEXT("Approach leaves the configured capsule clearance"),
		FVector::Dist2D(Player->GetActorLocation(), Target->GetActorLocation()) >= StopDistance - 1.0f);
	Anim->Montage_SetPosition(Montage, FirstHit + 0.01f);
	Tick(0.02f);
	TestEqual(TEXT("Hit window releases root-motion override"), Anim->RootMotionMode.GetValue(), OriginalRootMotionMode);
	TestEqual(TEXT("Approach restores walking"), Movement->MovementMode.GetValue(), MOVE_Walking);

	Reset();
	Target->SetActorLocation(FVector(StopDistance - 5.0f, 0.0f, Height));
	Start();
	Tick(0.1f);
	TestTrue(TEXT("Already close: no extra approach"), FMath::Abs(Player->GetActorLocation().X) < 0.1f);
	TestTrue(TEXT("Already close: facing is still corrected"), FMath::Abs(Player->GetActorRotation().Yaw) < 0.1f);

	Reset();
	Target->SetActorLocation(FVector(Data->AssistMaxDistance + 50.0f, 0.0f, Height));
	Start();
	Tick(0.1f);
	TestTrue(TEXT("Too far: no approach or turn"), FMath::Abs(Player->GetActorLocation().X) < 0.1f &&
		FMath::IsNearlyEqual(Player->GetActorRotation().Yaw, 30.0f, 0.1f));
	TestTrue(TEXT("Too far: the ordinary attack remains active"), ComboSpec->IsActive());

	Reset();
	AActor* Wall = Box(FVector(200.0f, 0.0f, Height), FVector(15.0f, 150.0f, 150.0f));
	Start();
	Tick(0.1f);
	TestTrue(TEXT("Wall blocks target assistance"), FMath::Abs(Player->GetActorLocation().X) < 0.1f);
	Wall->Destroy();

	Reset();
	TestTrue(TEXT("Unlocked attack activates"), Player->RequestComboAttack());
	Tick(0.1f);
	TestTrue(TEXT("Unlocked attack preserves movement and facing"), FMath::Abs(Player->GetActorLocation().X) < 0.1f &&
		FMath::IsNearlyEqual(Player->GetActorRotation().Yaw, 30.0f, 0.1f));

	Reset();
	Start();
	Tick(0.05f);
	TestTrue(TEXT("Approach is moving before unlock"), Player->GetActorLocation().X > 1.0f);
	Targeting->ClearTarget();
	const FVector UnlockedLocation = Player->GetActorLocation();
	Tick(0.1f);
	TestTrue(TEXT("Unlock immediately stops the approach"), Player->GetActorLocation().Equals(UnlockedLocation, 0.1f));
	TestEqual(TEXT("Unlock restores root-motion mode"), Anim->RootMotionMode.GetValue(), OriginalRootMotionMode);

	Reset();
	Start();
	Tick(0.05f);
	ASC->CancelAbilities(&ActionTags);
	const FVector CancelledLocation = Player->GetActorLocation();
	Tick(0.1f);
	TestTrue(TEXT("Cancellation stops approach"), Player->GetActorLocation().Equals(CancelledLocation, 0.1f));
	TestEqual(TEXT("Cancellation restores walking"), Movement->MovementMode.GetValue(), MOVE_Walking);
	TestEqual(TEXT("Cancellation restores root-motion mode"), Anim->RootMotionMode.GetValue(), OriginalRootMotionMode);
	TestFalse(TEXT("Cancellation releases the movement lock"), ASC->HasMatchingGameplayTag(AshenOathGameplayTags::State_MovementLocked));

	Reset();
	Start();
	Anim->Montage_JumpToSection(Data->ComboSections[1], Montage);
	Player->SetActorRotation(FRotator::ZeroRotator);
	Target->SetActorLocation(FVector(350.0f, 150.0f, Height));
	Tick(0.08f);
	TestTrue(TEXT("Middle sections receive no added turn or approach"), FMath::Abs(Player->GetActorLocation().X) < 0.1f &&
		FMath::Abs(Player->GetActorRotation().Yaw) < 0.1f);
	Anim->Montage_JumpToSection(Data->ComboSections[3], Montage);
	Tick(0.05f);
	const float FirstYaw = Player->GetActorRotation().Yaw;
	TestTrue(TEXT("Finisher windup turns toward the target"), FirstYaw > 1.0f);
	Target->SetActorLocation(FVector(300.0f, 250.0f, Height));
	Tick(0.05f);
	TestTrue(TEXT("Finisher tracks the target's updated bearing"), Player->GetActorRotation().Yaw > FirstYaw + 1.0f);
	TestTrue(TEXT("Finisher adds no displacement"), FMath::Abs(Player->GetActorLocation().X) < 0.1f);
	Anim->Montage_SetPosition(Montage, FinisherHit + 0.01f);
	const float CommittedYaw = Player->GetActorRotation().Yaw;
	Target->SetActorLocation(FVector(300.0f, -250.0f, Height));
	Tick(0.1f);
	TestTrue(TEXT("Finisher stops turning at the hit window"), FMath::IsNearlyEqual(Player->GetActorRotation().Yaw, CommittedYaw, 0.1f));

	Reset();
	Start();
	Anim->Montage_JumpToSection(Data->ComboSections[3], Montage);
	Target->SetActorLocation(FVector(350.0f, 150.0f, Height));
	Tick(0.03f);
	Target->GetAbilitySystemComponent()->AddLooseGameplayTag(AshenOathGameplayTags::State_Dead);
	const float DeadTargetYaw = Player->GetActorRotation().Yaw;
	Tick(0.05f);
	TestTrue(TEXT("Target death stops finisher tracking"), FMath::IsNearlyEqual(Player->GetActorRotation().Yaw, DeadTargetYaw, 0.1f));
	ASC->CancelAbilities(&ActionTags);
	return true;
}

#endif
