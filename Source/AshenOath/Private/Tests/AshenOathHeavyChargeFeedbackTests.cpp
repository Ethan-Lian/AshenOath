#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/Ability/AshenOathHeavyAttackAbility.h"
#include "AbilitySystem/AshenOathAttributeSet.h"
#include "AbilitySystem/Data/AshenOathHeavyAttackData.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Characters/AshenOathPlayerCharacter.h"
#include "Components/PointLightComponent.h"
#include "Components/ProgressBar.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Feedback/AshenOathCombatFeedbackComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTags/AshenOathGameplayTags.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Particles/ParticleSystemComponent.h"
#include "UI/AshenOathPlayerStatusWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAshenOathHeavyChargeFeedbackTest,
	"AshenOath.Combat.Ability.HeavyChargeFeedback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FAshenOathHeavyChargeFeedbackTest::RunTest(const FString& Parameters)
{
	TGuardValue<uint64> FrameCounterGuard(GFrameCounter, GFrameCounter);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();
	ON_SCOPE_EXIT
	{
		World->EndPlay(EEndPlayReason::Quit);
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	};
	UClass* PlayerClass = LoadClass<AAshenOathPlayerCharacter>(nullptr,
		TEXT("/Game/AshenOath/Characters/BP_AshenOathPlayer.BP_AshenOathPlayer_C"));
	if (!TestNotNull(TEXT("Configured player loads"), PlayerClass))
	{
		return false;
	}
	AAshenOathPlayerCharacter* Player = World->SpawnActor<AAshenOathPlayerCharacter>(PlayerClass);
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	Player->DispatchBeginPlay();
	Controller->Possess(Player);
	Player->SetActorTickEnabled(false);
	Player->GetCharacterMovement()->SetComponentTickEnabled(false);
	Player->GetMesh()->SetComponentTickEnabled(false);
	UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
	UAshenOathCombatFeedbackComponent* Feedback = Player->FindComponentByClass<UAshenOathCombatFeedbackComponent>();
	const UAshenOathHeavyAttackData* Data = nullptr;
	UAshenOathHeavyAttackAbility* Ability = nullptr;
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.Ability->IsA<UAshenOathHeavyAttackAbility>())
		{
			Data = Cast<UAshenOathHeavyAttackData>(Spec.SourceObject.Get());
			Ability = Cast<UAshenOathHeavyAttackAbility>(Spec.GetPrimaryInstance());
			break;
		}
	}
	if (!TestNotNull(TEXT("Heavy action configured"), Data) || !TestNotNull(TEXT("Feedback component exists"), Feedback))
	{
		return false;
	}
	TestNotNull(TEXT("Loop sound configured"), Data->ChargeLoopSound.Get());
	TestNotNull(TEXT("Ready sound configured"), Data->ChargeReadySound.Get());
	TestNotNull(TEXT("Staff particles configured"), Data->ChargeLoopEffect.Get());
	UClass* WidgetClass = LoadClass<UAshenOathPlayerStatusWidget>(nullptr,
		TEXT("/Game/AshenOath/UI/WBP_PlayerStatus.WBP_PlayerStatus_C"));
	UAshenOathPlayerStatusWidget* Widget = CreateWidget<UAshenOathPlayerStatusWidget>(World, WidgetClass);
	if (!TestNotNull(TEXT("Status widget creates"), Widget))
	{
		return false;
	}
	UProgressBar* ChargeBar = Cast<UProgressBar>(Widget->GetWidgetFromName(TEXT("ChargeBar")));
	UTextBlock* ChargeText = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("ChargeText")));
	if (!TestNotNull(TEXT("Charge bar is authored"), ChargeBar) || !TestNotNull(TEXT("Charge label is authored"), ChargeText))
	{
		return false;
	}
	FGameplayTagContainer ActionTags(AshenOathGameplayTags::Ability_Action);
	auto Reset = [&](const float Stamina)
	{
		ASC->CancelAbilities(&ActionTags);
		Player->GetMesh()->GetAnimInstance()->Montage_Stop(0.0f);
		ASC->SetNumericAttributeBase(UAshenOathAttributeSet::GetMaxStaminaAttribute(), Data->MaxChargeStaminaCost * 2.0f);
		ASC->SetNumericAttributeBase(UAshenOathAttributeSet::GetStaminaAttribute(), Stamina);
	};
	auto Tick = [World](const float Duration)
	{
		for (float Remaining = Duration; Remaining > KINDA_SMALL_NUMBER;)
		{
			const float Step = FMath::Min(Remaining, 1.0f / 120.0f);
			// A synchronous test must advance the frame identity for TimerManager.
			++GFrameCounter;
			World->Tick(LEVELTICK_All, Step);
			Remaining -= Step;
		}
	};
	int32 Changes = 0;
	const FDelegateHandle ChangeHandle = Player->OnHeavyChargeChanged().AddLambda([&Changes]() { ++Changes; });
	ON_SCOPE_EXIT { Player->OnHeavyChargeChanged().Remove(ChangeHandle); };
	auto CheckStaffAlignment = [&](const UParticleSystem* Effect)
	{
		const USkeletalMeshComponent* Mesh = Player->GetMesh();
		const FVector StaffAxis = (Mesh->GetSocketLocation(TEXT("FX_Staff_Tip_B")) -
			Mesh->GetSocketLocation(TEXT("FX_Staff_Tip_A"))).GetSafeNormal();
		TInlineComponentArray<UParticleSystemComponent*> Particles(Player);
		int32 Found = 0;
		for (const UParticleSystemComponent* Particle : Particles)
		{
			if (Particle->Template == Effect)
			{
				++Found;
				// The authored staff particles extend along their local X axis.
				TestTrue(TEXT("Particle long axis follows the actual staff tips"),
					FMath::Abs(FVector::DotProduct(Particle->GetForwardVector(), StaffAxis)) > 0.99f);
			}
		}
		TestTrue(TEXT("The requested staff effect exists for alignment validation"), Found > 0);
	};

	Reset(Data->MaxChargeStaminaCost);
	TestTrue(TEXT("Charge activates"), Player->RequestHeavyAttackPressed());
	TestTrue(TEXT("Charge is visible immediately"), Player->GetHeavyChargeState().bCharging);
	Tick(0.5f);
	TestFalse(TEXT("Early charge cannot release"), Player->GetHeavyChargeState().bCanRelease);
	TestTrue(TEXT("Progress reflects paid stamina"), Player->GetHeavyChargeState().ChargeAlpha > 0.0f);
	Widget->ObservePlayer(Player);
	TestEqual(TEXT("Late UI observer receives current progress"), ChargeBar->GetPercent(), Player->GetHeavyChargeState().ChargeAlpha);
	TestEqual(TEXT("Charging UI is visible"), ChargeBar->GetVisibility(), ESlateVisibility::HitTestInvisible);
	TestFalse(TEXT("Early release cancels"), Player->RequestHeavyAttackReleased());
	TestFalse(TEXT("Cancellation hides charge"), Player->GetHeavyChargeState().bCharging);
	TestEqual(TEXT("Cancellation collapses progress bar"), ChargeBar->GetVisibility(), ESlateVisibility::Collapsed);
	TestNull(TEXT("Cancellation removes charge light"), Player->FindComponentByClass<UPointLightComponent>());

	Reset(Data->MaxChargeStaminaCost);
	Player->RequestHeavyAttackPressed();
	Tick(1.25f);
	TestTrue(TEXT("One second unlocks release"), Player->GetHeavyChargeState().bCanRelease);
	TestTrue(TEXT("Ready is distinct from full power"), Player->GetHeavyChargeState().ChargeAlpha < 1.0f);
	TestEqual(TEXT("Ready hint changes once eligible"), ChargeText->GetText().ToString(), FString(TEXT("松开出招")));
	for (const FName Section : {Data->ChargeStartSection, Data->ChargeLoopSection})
	{
		float SectionStart = 0.0f;
		float SectionEnd = 0.0f;
		Data->Montage->GetSectionStartAndEndTime(Data->Montage->GetSectionIndex(Section), SectionStart, SectionEnd);
		Player->GetMesh()->GetAnimInstance()->Montage_SetPosition(Data->Montage, (SectionStart + SectionEnd) * 0.5f);
		Player->GetMesh()->TickAnimation(0.0f, false);
		Player->GetMesh()->RefreshBoneTransforms();
		Player->SetActorRotation(FRotator(0.0f, Section == Data->ChargeStartSection ? 30.0f : 135.0f, 0.0f));
		CheckStaffAlignment(Data->ChargeLoopEffect);
		CheckStaffAlignment(Data->ChargeReadyEffect);
	}
	Player->RequestDodge(FVector2D(0.0f, 1.0f));
	TestFalse(TEXT("Dodge cancels charging"), Player->GetHeavyChargeState().bCharging);
	TestNull(TEXT("Dodge removes charge light"), Player->FindComponentByClass<UPointLightComponent>());

	Reset(Data->MaxChargeStaminaCost);
	Player->RequestHeavyAttackPressed();
	Tick(1.25f);
	TestTrue(TEXT("Eligible manual release starts the attack"), Player->RequestHeavyAttackReleased());
	TestNull(TEXT("Manual release stops the loop immediately"), Player->FindComponentByClass<UPointLightComponent>());
	const FAnimMontageInstance* Instance = Player->GetMesh()->GetAnimInstance()->GetActiveInstanceForMontage(Data->Montage);
	auto CountReleaseParticles = [&]()
	{
		TInlineComponentArray<UParticleSystemComponent*> Particles(Player);
		int32 Count = 0;
		for (const UParticleSystemComponent* Particle : Particles)
		{
			if (Particle->Template == Data->ChargeReleaseEffect)
			{
				++Count;
			}
		}
		return Count;
	};
	if (TestNotNull(TEXT("Release retains its montage execution"), Instance) && TestNotNull(TEXT("Heavy instance exists"), Ability))
	{
		Ability->RequestReleaseFeedbackFromAnimation(Player->GetMesh(), Data->Montage, Instance->GetInstanceID() + 1);
		TestEqual(TEXT("An old montage cannot play release feedback"), CountReleaseParticles(), 0);
		Ability->RequestReleaseFeedbackFromAnimation(Player->GetMesh(), Data->Montage, Instance->GetInstanceID());
		TestEqual(TEXT("Owned notify plays the release burst"), CountReleaseParticles(), 1);
		CheckStaffAlignment(Data->ChargeReleaseEffect);
		Ability->RequestReleaseFeedbackFromAnimation(Player->GetMesh(), Data->Montage, Instance->GetInstanceID());
		TestEqual(TEXT("Duplicate notify does not repeat the release burst"), CountReleaseParticles(), 1);
	}

	const float CostPerSecond = Data->MaxChargeStaminaCost / Data->MaxChargeDurationSeconds;
	Reset(CostPerSecond * 0.3f);
	Player->RequestHeavyAttackPressed();
	Tick(0.6f);
	TestFalse(TEXT("Exhaustion before minimum cancels"), Player->GetHeavyChargeState().bCharging);
	TestNull(TEXT("Exhaustion cancellation cleans effects"), Player->FindComponentByClass<UPointLightComponent>());

	Reset(CostPerSecond * 1.4f);
	Player->RequestHeavyAttackPressed();
	Tick(1.7f);
	TestFalse(TEXT("Exhaustion after minimum releases"), Player->GetHeavyChargeState().bCharging);
	TestNull(TEXT("Release stops sustained effects"), Player->FindComponentByClass<UPointLightComponent>());

	Reset(Data->MaxChargeStaminaCost * 2.0f);
	Player->RequestHeavyAttackPressed();
	Tick(Data->MaxChargeDurationSeconds + 0.15f);
	TestFalse(TEXT("Maximum duration automatically releases"), Player->GetHeavyChargeState().bCharging);
	TestNull(TEXT("Full release stops sustained effects"), Player->FindComponentByClass<UPointLightComponent>());

	Reset(Data->MaxChargeStaminaCost);
	const uint32 OldId = Feedback->BeginChargeFeedback(Data);
	const uint32 NewId = Feedback->BeginChargeFeedback(Data);
	Feedback->EndChargeFeedback(OldId, true);
	TestNotNull(TEXT("Old cleanup cannot stop a new charge"), Player->FindComponentByClass<UPointLightComponent>());
	Feedback->EndChargeFeedback(NewId, true);
	TestNull(TEXT("Current cleanup stops its own charge"), Player->FindComponentByClass<UPointLightComponent>());

	Player->RequestHeavyAttackPressed();
	Player->StopCombatForMatchEnd();
	TestFalse(TEXT("Match end hides charge"), Player->GetHeavyChargeState().bCharging);
	TestNull(TEXT("Match end removes charge effects"), Player->FindComponentByClass<UPointLightComponent>());
	TestTrue(TEXT("Observers receive lifecycle and progress updates"), Changes >= 8);
	Widget->ObservePlayer(nullptr);
	return true;
}

#endif
