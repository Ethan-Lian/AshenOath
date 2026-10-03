#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/AshenOathAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Characters/AshenOathBossCharacter.h"
#include "Characters/AshenOathPlayerCharacter.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StateTreeComponent.h"
#include "Damage/CombatDamageComponent.h"
#include "Death/CombatDeathComponent.h"
#include "Defense/CombatDefenseComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Game/AshenOathGameMode.h"
#include "GameplayEffect.h"
#include "GameplayTags/AshenOathGameplayTags.h"
#include "LevelSequence.h"
#include "LevelSequencePlayer.h"
#include "Misc/AutomationTest.h"
#include "MovieScene.h"
#include "Player/AshenOathPlayerController.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundWaveProcedural.h"
#include "UObject/UnrealType.h"

namespace AshenOathDeathAndMatchTests
{
	UWorld* CreateGameWorld(bool bWithMatchGameMode = false)
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		FWorldContext& WorldContext =
			GEngine->CreateNewWorldContext(EWorldType::Game);
		WorldContext.SetCurrentWorld(World);

		FURL URL;
		if (bWithMatchGameMode)
		{
			UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
			WorldContext.OwningGameInstance = GameInstance;
			World->SetGameInstance(GameInstance);
			URL.AddOption(TEXT("game=/Script/AshenOath.AshenOathGameMode"));
			World->SetGameMode(URL);
		}
		World->InitializeActorsForPlay(URL);
		World->BeginPlay();
		return World;
	}

	void DestroyGameWorld(UWorld* World)
	{
		if (!World)
		{
			return;
		}

		World->EndPlay(EEndPlayReason::Quit);
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	}

	bool SpawnMatchActors(
		UWorld& World,
		AAshenOathPlayerController*& OutController,
		AAshenOathPlayerCharacter*& OutPlayer,
		AAshenOathBossCharacter*& OutBoss,
		UClass* PlayerClass = AAshenOathPlayerCharacter::StaticClass(),
		UClass* BossClass = AAshenOathBossCharacter::StaticClass())
	{
		OutController = World.SpawnActor<AAshenOathPlayerController>();
		OutPlayer = World.SpawnActor<AAshenOathPlayerCharacter>(PlayerClass);
		OutBoss = World.SpawnActor<AAshenOathBossCharacter>(BossClass);

		if (!OutController || !OutPlayer || !OutBoss)
		{
			return false;
		}

		// Transient automation worlds do not consistently dispatch BeginPlay for
		// actors spawned after UWorld::BeginPlay. The production bindings and tag
		// configuration under test are established there, so mirror the normal
		// game-world lifecycle when the test world has not done it for us.
		for (AActor* Actor : {static_cast<AActor*>(OutController), static_cast<AActor*>(OutPlayer), static_cast<AActor*>(OutBoss)})
		{
			if (!Actor->HasActorBegunPlay())
			{
				Actor->DispatchBeginPlay();
			}
		}

		OutController->Possess(OutPlayer);
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAshenOathDeathLifecycleTest,
	"AshenOath.Combat.Lifecycle.Death",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FAshenOathDeathLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace AshenOathDeathAndMatchTests;

	UWorld* World = CreateGameWorld();
	AAshenOathPlayerController* PlayerController = nullptr;
	AAshenOathPlayerCharacter* Player = nullptr;
	AAshenOathBossCharacter* Boss = nullptr;

	TestTrue(
		TEXT("Match actors spawn"),
		SpawnMatchActors(*World, PlayerController, Player, Boss)
	);

	if (!PlayerController || !Player || !Boss)
	{
		DestroyGameWorld(World);
		return false;
	}

	UAbilitySystemComponent* PlayerAbilitySystem =
		Player->GetAbilitySystemComponent();
	UCombatDeathComponent* Death =
		Player->FindComponentByClass<UCombatDeathComponent>();
	UCombatDefenseComponent* Defense =
		Player->FindComponentByClass<UCombatDefenseComponent>();
	UCombatDamageComponent* Damage =
		Player->FindComponentByClass<UCombatDamageComponent>();

	TestNotNull(TEXT("Player owns an ASC"), PlayerAbilitySystem);
	TestNotNull(TEXT("Player owns CombatDeath"), Death);
	TestNotNull(TEXT("Player owns CombatDefense"), Defense);
	TestNotNull(TEXT("Player owns CombatDamage"), Damage);

	if (!PlayerAbilitySystem || !Death || !Defense || !Damage)
	{
		DestroyGameWorld(World);
		return false;
	}

	PlayerAbilitySystem->SetNumericAttributeBase(
		UAshenOathAttributeSet::GetMaxHealthAttribute(),
		100.0f
	);
	PlayerAbilitySystem->SetNumericAttributeBase(
		UAshenOathAttributeSet::GetHealthAttribute(),
		100.0f
	);
	TestTrue(TEXT("Death observation initializes"), Death->Initialize(PlayerAbilitySystem));

	int32 DeathEventCount = 0;
	int32 PresentationFinishedCount = 0;
	const FDelegateHandle PresentationHandle = Death->OnDeathPresentationFinished().AddLambda(
		[&PresentationFinishedCount]() { ++PresentationFinishedCount; }
	);
	const FDelegateHandle DeathHandle = Death->OnDeathStarted().AddLambda(
		[&DeathEventCount]()
		{
			++DeathEventCount;
		}
	);

	FGameplayTagContainer DefenseTags;
	DefenseTags.AddTag(AshenOathGameplayTags::State_Invulnerable);
	const FCombatDefenseWindowHandle DefenseHandle = Defense->BeginDodgeWindow(
		Player,
		DefenseTags,
		0.0f,
		1.0f,
		World->GetTimeSeconds()
	);
	TestTrue(TEXT("Pre-death defense window starts"), DefenseHandle.IsValid());
	TestTrue(TEXT("Pre-death defense window activates"), Defense->ActivateDodgeWindow(DefenseHandle));

	PlayerAbilitySystem->SetNumericAttributeBase(
		UAshenOathAttributeSet::GetHealthAttribute(),
		0.0f
	);

	TestTrue(TEXT("Zero health starts death"), Death->IsDeathStarted());
	TestEqual(TEXT("Death broadcasts once"), DeathEventCount, 1);
	TestEqual(TEXT("Missing Montage finishes presentation without waiting"), PresentationFinishedCount, 1);
	TestEqual(
		TEXT("Death owns one terminal tag"),
		PlayerAbilitySystem->GetTagCount(AshenOathGameplayTags::State_Dead),
		1
	);
	TestFalse(TEXT("Death clears the defense window"), Defense->OwnsWindow(DefenseHandle));
	TestEqual(
		TEXT("Death removes defense-owned invulnerability"),
		PlayerAbilitySystem->GetTagCount(AshenOathGameplayTags::State_Invulnerable),
		0
	);

	PlayerAbilitySystem->SetNumericAttributeBase(
		UAshenOathAttributeSet::GetHealthAttribute(),
		50.0f
	);
	PlayerAbilitySystem->SetNumericAttributeBase(
		UAshenOathAttributeSet::GetHealthAttribute(),
		0.0f
	);
	TestEqual(TEXT("A dead actor cannot transition twice"), DeathEventCount, 1);
	TestEqual(TEXT("Presentation completion also broadcasts once"), PresentationFinishedCount, 1);
	TestEqual(
		TEXT("Repeated zero health does not duplicate the terminal tag"),
		PlayerAbilitySystem->GetTagCount(AshenOathGameplayTags::State_Dead),
		1
	);

	FCombatDamageAttempt DamageAttempt;
	DamageAttempt.SourceActor = Boss;
	DamageAttempt.DamageEffect = UGameplayEffect::StaticClass();
	DamageAttempt.HitTimeSeconds = World->GetTimeSeconds();
	TestEqual(
		TEXT("Terminal actors reject further damage"),
		Damage->ApplyDamageAttempt(DamageAttempt),
		ECombatDamageResult::Invalid
	);

	Death->OnDeathStarted().Remove(DeathHandle);
	Death->OnDeathPresentationFinished().Remove(PresentationHandle);
	DestroyGameWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAshenOathMatchOutcomeTest,
	"AshenOath.Combat.Lifecycle.MatchOutcome",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FAshenOathMatchOutcomeTest::RunTest(const FString& Parameters)
{
	using namespace AshenOathDeathAndMatchTests;

	for (const bool bBossDiesFirst : {false, true})
	{
		UWorld* World = CreateGameWorld();
		AAshenOathGameMode* GameMode =
			World->SpawnActor<AAshenOathGameMode>();
		AAshenOathPlayerController* PlayerController = nullptr;
		AAshenOathPlayerCharacter* Player = nullptr;
		AAshenOathBossCharacter* Boss = nullptr;

		TestNotNull(TEXT("Test world owns AshenOath GameMode"), GameMode);
		TestTrue(
			TEXT("Match actors spawn"),
			SpawnMatchActors(*World, PlayerController, Player, Boss)
		);

		if (!GameMode || !PlayerController || !Player || !Boss)
		{
			DestroyGameWorld(World);
			return false;
		}

		TestTrue(TEXT("Boss registers as active"), GameMode->RegisterBoss(Boss));
		PlayerController->SetAsLocalPlayerController();

		int32 OutcomeEventCount = 0;
		EAshenOathMatchOutcome ObservedOutcome =
			EAshenOathMatchOutcome::InProgress;
		const FDelegateHandle OutcomeHandle =
			GameMode->OnMatchOutcomeChanged().AddLambda(
				[&OutcomeEventCount, &ObservedOutcome](
					const EAshenOathMatchOutcome Outcome)
				{
					++OutcomeEventCount;
					ObservedOutcome = Outcome;
				}
			);

		const EAshenOathMatchOutcome ExpectedOutcome = bBossDiesFirst
			? EAshenOathMatchOutcome::Victory
			: EAshenOathMatchOutcome::Defeat;
		int32 PresentationCount = 0;
		const FDelegateHandle PresentationHandle = GameMode->OnMatchPresentationReady().AddLambda(
			[&](EAshenOathMatchOutcome Outcome)
			{
				++PresentationCount;
				TestEqual(TEXT("Presentation uses the locked outcome"), Outcome, ExpectedOutcome);
				TestTrue(TEXT("Presentation snapshot is ready before notifying HUD"), GameMode->IsMatchPresentationReady());
			});

		if (bBossDiesFirst)
		{
			GameMode->ReportBossDeath(Boss);
			GameMode->ReportBossDeath(Boss);
			GameMode->ReportPlayerDeath(Player);
		}
		else
		{
			GameMode->ReportPlayerDeath(Player);
			GameMode->ReportPlayerDeath(Player);
			GameMode->ReportBossDeath(Boss);
		}

		TestEqual(TEXT("First terminal report owns the outcome"), GameMode->GetMatchOutcome(), ExpectedOutcome);
		TestEqual(TEXT("Outcome listener observes the committed value"), ObservedOutcome, ExpectedOutcome);
		TestEqual(TEXT("Outcome broadcasts exactly once"), OutcomeEventCount, 1);
		TestFalse(TEXT("Resolving the match does not immediately show UI"), GameMode->IsMatchPresentationReady());
		TestFalse(TEXT("Retry is rejected during death presentation"), GameMode->RequestRetry(PlayerController));
		TestFalse(TEXT("Cursor stays hidden during death presentation"), PlayerController->ShouldShowMouseCursor());
		GameMode->ReportDeathPresentationFinished(bBossDiesFirst ? static_cast<AActor*>(Player) : Boss);
		TestEqual(TEXT("The other actor cannot release the result UI"), PresentationCount, 0);
		AActor* DeadActor = bBossDiesFirst ? static_cast<AActor*>(Boss) : Player;
		GameMode->ReportDeathPresentationFinished(DeadActor);
		GameMode->ReportDeathPresentationFinished(DeadActor);
		TestFalse(TEXT("The post-Montage hold keeps UI hidden"), GameMode->IsMatchPresentationReady());
		for (int32 Frame = 0; Frame < 8; ++Frame)
		{
			++GFrameCounter;
			World->Tick(LEVELTICK_All, 0.1f);
		}
		TestTrue(TEXT("The result UI becomes ready after the hold"), GameMode->IsMatchPresentationReady());
		TestEqual(TEXT("Presentation broadcasts exactly once"), PresentationCount, 1);
		TestTrue(TEXT("Cursor is available for Retry once UI is ready"), PlayerController->ShouldShowMouseCursor());
		GameMode->ReportDeathPresentationFinished(DeadActor);
		TestEqual(TEXT("Late completion cannot repeat presentation"), PresentationCount, 1);

		GameMode->OnMatchOutcomeChanged().Remove(OutcomeHandle);
		GameMode->OnMatchPresentationReady().Remove(PresentationHandle);
		DestroyGameWorld(World);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAshenOathDeathPresentationTest,
	"AshenOath.Combat.Lifecycle.DeathPresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FAshenOathDeathPresentationTest::RunTest(const FString& Parameters)
{
	using namespace AshenOathDeathAndMatchTests;
	UClass* PlayerClass = LoadClass<AAshenOathPlayerCharacter>(nullptr,
		TEXT("/Game/AshenOath/Characters/BP_AshenOathPlayer.BP_AshenOathPlayer_C"));
	UClass* BossClass = LoadClass<AAshenOathBossCharacter>(nullptr,
		TEXT("/Game/AshenOath/Characters/BP_AshenOathBoss.BP_AshenOathBoss_C"));
	FObjectProperty* MontageProperty = FindFProperty<FObjectProperty>(UCombatDeathComponent::StaticClass(), TEXT("DeathMontage"));
	if (!PlayerClass || !BossClass || !MontageProperty)
	{
		AddError(TEXT("Configured characters and death Montage property must load"));
		return false;
	}

	enum class EPresentationExit { Finished, Interrupted, MissingMontage, FailedPlayback, WorldExit };
	for (const bool bBossDies : {false, true})
	{
		for (const EPresentationExit Exit : {EPresentationExit::Finished, EPresentationExit::Interrupted,
			EPresentationExit::MissingMontage, EPresentationExit::FailedPlayback, EPresentationExit::WorldExit})
		{
			UWorld* World = CreateGameWorld(true);
			AAshenOathGameMode* GameMode = World->GetAuthGameMode<AAshenOathGameMode>();
			AAshenOathPlayerController* Controller = nullptr;
			AAshenOathPlayerCharacter* Player = nullptr;
			AAshenOathBossCharacter* Boss = nullptr;
			if (!GameMode || !SpawnMatchActors(*World, Controller, Player, Boss, PlayerClass, BossClass))
			{
				AddError(TEXT("Production death presentation actors must spawn"));
				DestroyGameWorld(World);
				return false;
			}
			GameMode->RegisterBoss(Boss);
			ACharacter* DeadCharacter = bBossDies ? static_cast<ACharacter*>(Boss) : Player;
			UAbilitySystemComponent* ASC = bBossDies ? Boss->GetAbilitySystemComponent() : Player->GetAbilitySystemComponent();
			UCombatDeathComponent* Death = DeadCharacter->FindComponentByClass<UCombatDeathComponent>();
			USkeletalMeshComponent* Mesh = DeadCharacter->GetMesh();
			Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
			UAnimMontage* Montage = Cast<UAnimMontage>(MontageProperty->GetObjectPropertyValue_InContainer(Death));
			if (Exit == EPresentationExit::MissingMontage)
			{
				MontageProperty->SetObjectPropertyValue_InContainer(Death, nullptr);
			}
			else if (Exit == EPresentationExit::FailedPlayback)
			{
				// A configured zero-length Montage exercises the engine's playback failure.
				MontageProperty->SetObjectPropertyValue_InContainer(Death, NewObject<UAnimMontage>(Death));
			}
			else if (!Montage || !Mesh->GetAnimInstance())
			{
				AddError(TEXT("Configured death Montage and AnimInstance must be available"));
				DestroyGameWorld(World);
				return false;
			}

			int32 FinishedCount = 0;
			int32 ReadyCount = 0;
			const FDelegateHandle FinishedHandle = Death->OnDeathPresentationFinished().AddLambda([&]() { ++FinishedCount; });
			const FDelegateHandle ReadyHandle = GameMode->OnMatchPresentationReady().AddLambda(
				[&](EAshenOathMatchOutcome) { ++ReadyCount; });
			ASC->SetNumericAttributeBase(UAshenOathAttributeSet::GetHealthAttribute(), 100.0f);
			Death->Initialize(ASC);
			ASC->SetNumericAttributeBase(UAshenOathAttributeSet::GetHealthAttribute(), 0.0f);
			TestEqual(TEXT("Character immediately reports the death outcome"), GameMode->GetMatchOutcome(),
				bBossDies ? EAshenOathMatchOutcome::Victory : EAshenOathMatchOutcome::Defeat);
			TestFalse(TEXT("Production death does not immediately cover the scene"), GameMode->IsMatchPresentationReady());
			TestFalse(TEXT("Match end stops the Boss StateTree"), Boss->FindComponentByClass<UStateTreeComponent>()->IsRunning());

			if (Exit == EPresentationExit::WorldExit)
			{
				World->EndPlay(EEndPlayReason::Quit);
				TestEqual(TEXT("Teardown does not report death presentation completion"), FinishedCount, 0);
				TestEqual(TEXT("Teardown does not show the result UI"), ReadyCount, 0);
			}
			else
			{
				if (Exit == EPresentationExit::Finished || Exit == EPresentationExit::Interrupted)
				{
					TestEqual(TEXT("A playing Montage does not finish synchronously"), FinishedCount, 0);
					TestTrue(TEXT("The configured death Montage is playing"), Mesh->GetAnimInstance()->Montage_IsPlaying(Montage));
					if (Exit == EPresentationExit::Interrupted)
					{
						Mesh->GetAnimInstance()->Montage_Stop(0.0f, Montage);
					}
				}
				else
				{
					TestEqual(TEXT("Missing or failed Montage completes without blocking UI"), FinishedCount, 1);
				}
				for (int32 Frame = 0; Frame < 400 && !GameMode->IsMatchPresentationReady(); ++Frame)
				{
					++GFrameCounter;
					World->Tick(LEVELTICK_All, 0.05f);
				}
				TestEqual(TEXT("Death presentation completes once"), FinishedCount, 1);
				TestEqual(TEXT("Character forwards completion and GameMode releases UI once"), ReadyCount, 1);
				TestTrue(TEXT("Result UI becomes ready"), GameMode->IsMatchPresentationReady());
			}
			Death->OnDeathPresentationFinished().Remove(FinishedHandle);
			GameMode->OnMatchPresentationReady().Remove(ReadyHandle);
			DestroyGameWorld(World);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAshenOathBossIntroLifecycleTest,
	"AshenOath.Combat.Lifecycle.BossIntro",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FAshenOathBossIntroLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace AshenOathDeathAndMatchTests;

	enum class EIntroExit
	{
		Finished,
		Stopped,
		Defeat,
		WorldExit,
		DefeatDuringStart,
		NoSequence
	};

	FObjectProperty* SequenceProperty = FindFProperty<FObjectProperty>(AAshenOathGameMode::StaticClass(), TEXT("IntroSequence"));
	FObjectProperty* PlayerProperty = FindFProperty<FObjectProperty>(AAshenOathGameMode::StaticClass(), TEXT("IntroPlayer"));
	FObjectProperty* MixProperty = FindFProperty<FObjectProperty>(AAshenOathGameMode::StaticClass(), TEXT("IntroSoundMix"));
	if (!SequenceProperty || !PlayerProperty || !MixProperty)
	{
		AddError(TEXT("Intro configuration and playback properties must be reflected"));
		return false;
	}

	for (const EIntroExit Exit : {EIntroExit::Finished, EIntroExit::Stopped, EIntroExit::Defeat,
		EIntroExit::WorldExit, EIntroExit::DefeatDuringStart, EIntroExit::NoSequence})
	{
		UWorld* World = CreateGameWorld();
		AAshenOathGameMode* GameMode = World->SpawnActor<AAshenOathGameMode>();
		AAshenOathPlayerController* Controller = nullptr;
		AAshenOathPlayerCharacter* Player = nullptr;
		AAshenOathBossCharacter* Boss = nullptr;
		if (!GameMode || !SpawnMatchActors(*World, Controller, Player, Boss))
		{
			AddError(TEXT("Intro match actors must spawn"));
			DestroyGameWorld(World);
			return false;
		}
		// This isolated world has no viewport player; mark the controller as local
		// through the same engine entry point used when assigning a LocalPlayer.
		Controller->SetAsLocalPlayerController();
		if (!GameMode->HasActorBegunPlay())
		{
			GameMode->DispatchBeginPlay();
		}
		GameMode->RegisterBoss(Boss);

		if (Exit != EIntroExit::NoSequence)
		{
			ULevelSequence* Sequence = NewObject<ULevelSequence>(GameMode);
			Sequence->Initialize();
			Sequence->GetMovieScene()->SetTickResolutionDirectly(FFrameRate(24, 1));
			Sequence->GetMovieScene()->SetPlaybackRange(0, 24);
			SequenceProperty->SetObjectPropertyValue_InContainer(GameMode, Sequence);
		}
		MixProperty->SetObjectPropertyValue_InContainer(GameMode, NewObject<USoundMix>(GameMode));

		TArray<bool> PresentationChanges;
		const FDelegateHandle IntroHandle = GameMode->OnIntroPlaybackChanged().AddLambda(
			[&, GameMode, Player](const bool bPlaying)
			{
				PresentationChanges.Add(bPlaying);
				TestEqual(TEXT("Intro notification agrees with the committed snapshot"), GameMode->IsIntroPlaying(), bPlaying);
				if (bPlaying && Exit == EIntroExit::DefeatDuringStart)
				{
					GameMode->ReportPlayerDeath(Player);
				}
			});

		TestTrue(TEXT("The controlled player can trigger the intro"), GameMode->TryStartBossIntro(Player));
		TestFalse(TEXT("The entrance cannot retrigger"), GameMode->TryStartBossIntro(Player));

		if (Exit != EIntroExit::NoSequence && Exit != EIntroExit::DefeatDuringStart)
		{
			TestTrue(TEXT("Configured intro owns presentation"), GameMode->IsIntroPlaying());
			TestTrue(TEXT("Intro disables movement input"), Controller->IsMoveInputIgnored());
			ULevelSequencePlayer* IntroPlayer = Cast<ULevelSequencePlayer>(PlayerProperty->GetObjectPropertyValue_InContainer(GameMode));
			if (!IntroPlayer)
			{
				GameMode->OnIntroPlaybackChanged().Remove(IntroHandle);
				DestroyGameWorld(World);
				return false;
			}

			switch (Exit)
			{
			case EIntroExit::Finished:
				for (int32 Tick = 0; Tick < 15; ++Tick)
				{
					World->Tick(LEVELTICK_All, 0.1f);
				}
				break;
			case EIntroExit::Stopped:
				IntroPlayer->Stop();
				break;
			case EIntroExit::Defeat:
				GameMode->ReportPlayerDeath(Player);
				break;
			case EIntroExit::WorldExit:
				GameMode->Destroy();
				break;
			default:
				break;
			}
		}

		TestFalse(TEXT("Every intro exit releases presentation"), GameMode->IsIntroPlaying());
		const bool bExpectedCombat = Exit == EIntroExit::Finished || Exit == EIntroExit::Stopped || Exit == EIntroExit::NoSequence;
		TestEqual(TEXT("Only a normal finish, stop, or absent sequence starts combat"), GameMode->IsCombatActive(), bExpectedCombat);
		if (Exit != EIntroExit::WorldExit)
		{
			TestEqual(TEXT("Only combat restores movement input"), Controller->IsMoveInputIgnored(), !bExpectedCombat);
		}
		if (Exit == EIntroExit::NoSequence)
		{
			TestTrue(TEXT("An absent sequence never hides UI or enters cinematic mixing"), PresentationChanges.IsEmpty());
		}
		else
		{
			TestEqual(TEXT("Presentation enters and exits once"), PresentationChanges.Num(), 2);
			if (PresentationChanges.Num() == 2)
			{
				TestTrue(TEXT("The first presentation notification hides gameplay UI"), PresentationChanges[0]);
				TestFalse(TEXT("The final presentation notification restores gameplay UI"), PresentationChanges[1]);
			}
		}

		GameMode->OnIntroPlaybackChanged().Remove(IntroHandle);
		DestroyGameWorld(World);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAshenOathCombatMusicLifecycleTest,
	"AshenOath.Combat.Lifecycle.CombatMusic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FAshenOathCombatMusicLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace AshenOathDeathAndMatchTests;
	FObjectProperty* MusicProperty = FindFProperty<FObjectProperty>(AAshenOathGameMode::StaticClass(), TEXT("CombatMusic"));
	FObjectProperty* ComponentProperty = FindFProperty<FObjectProperty>(AAshenOathGameMode::StaticClass(), TEXT("CombatMusicComponent"));
	if (!MusicProperty || !ComponentProperty)
	{
		AddError(TEXT("Combat music configuration and ownership must be reflected"));
		return false;
	}

	if (!GEngine->UseSound() || !GEngine->GetMainAudioDevice())
	{
		AddWarning(TEXT("Combat music playback requires an audio device; run with -muteaudio instead of -nosound"));
		return true;
	}

	enum class EMusicExit { Victory, Defeat, ExitDuringFade, NoMusic };
	for (const EMusicExit Exit : {EMusicExit::Victory, EMusicExit::Defeat, EMusicExit::ExitDuringFade, EMusicExit::NoMusic})
	{
		UWorld* World = CreateGameWorld();
		AAshenOathGameMode* GameMode = World->SpawnActor<AAshenOathGameMode>();
		AAshenOathPlayerController* Controller = nullptr;
		AAshenOathPlayerCharacter* Player = nullptr;
		AAshenOathBossCharacter* Boss = nullptr;
		if (!GameMode || !SpawnMatchActors(*World, Controller, Player, Boss))
		{
			AddError(TEXT("Music match actors must spawn"));
			DestroyGameWorld(World);
			return false;
		}
		Controller->SetAsLocalPlayerController();
		if (!GameMode->HasActorBegunPlay())
		{
			GameMode->DispatchBeginPlay();
		}
		GameMode->RegisterBoss(Boss);

		if (Exit != EMusicExit::NoMusic)
		{
			// Silence exercises the real audio device and fade lifecycle without an asset dependency.
			USoundWaveProcedural* Music = NewObject<USoundWaveProcedural>(GameMode);
			Music->SetSampleRate(48000);
			Music->NumChannels = 1;
			Music->Duration = 10.0f;
			Music->bLooping = true;
			TArray<uint8> Silence;
			Silence.SetNumZeroed(48000 * sizeof(int16) * 10);
			Music->QueueAudio(Silence.GetData(), Silence.Num());
			MusicProperty->SetObjectPropertyValue_InContainer(GameMode, Music);
		}

		TestNull(TEXT("Music is absent before entering combat"), ComponentProperty->GetObjectPropertyValue_InContainer(GameMode));
		TestTrue(TEXT("The entrance starts combat"), GameMode->TryStartBossIntro(Player));
		TestTrue(TEXT("Music configuration does not block combat"), GameMode->IsCombatActive());
		UAudioComponent* MusicComponent = Cast<UAudioComponent>(ComponentProperty->GetObjectPropertyValue_InContainer(GameMode));
		if (Exit == EMusicExit::NoMusic)
		{
			TestNull(TEXT("An unconfigured match does not allocate a music component"), MusicComponent);
			DestroyGameWorld(World);
			continue;
		}
		if (!TestNotNull(TEXT("A configured match creates music"), MusicComponent))
		{
			DestroyGameWorld(World);
			return false;
		}

		TestEqual(TEXT("Music fades in when combat starts"), MusicComponent->GetPlayState(), EAudioComponentPlayState::FadingIn);
		TestFalse(TEXT("Music is independent of actor distance"), static_cast<bool>(MusicComponent->bAllowSpatialization));
		TestFalse(TEXT("Music cannot survive a level transition"), static_cast<bool>(MusicComponent->bIgnoreForFlushing));
		TestEqual(TEXT("Music starts at a subdued volume"), MusicComponent->VolumeMultiplier, 0.25f);
		TestFalse(TEXT("Reentering the trigger cannot restart music"), GameMode->TryStartBossIntro(Player));
		TestEqual(TEXT("The match keeps its original music component"),
			ComponentProperty->GetObjectPropertyValue_InContainer(GameMode), static_cast<UObject*>(MusicComponent));

		if (Exit == EMusicExit::Defeat)
		{
			GameMode->ReportPlayerDeath(Player);
		}
		else
		{
			GameMode->ReportBossDeath(Boss);
		}
		TestFalse(TEXT("Music fades before the delayed result UI"), GameMode->IsMatchPresentationReady());
		TestEqual(TEXT("Either outcome immediately starts the fade"), MusicComponent->GetPlayState(), EAudioComponentPlayState::FadingOut);

		if (Exit == EMusicExit::ExitDuringFade)
		{
			GameMode->Destroy();
			TestNull(TEXT("Exit releases music ownership during the fade"), ComponentProperty->GetObjectPropertyValue_InContainer(GameMode));
			TestFalse(TEXT("Exit unregisters the owned audio component"), MusicComponent->IsRegistered());
			TestFalse(TEXT("Exit immediately stops the fading music"), MusicComponent->IsPlaying());
			DestroyGameWorld(World);
		}
		else
		{
			ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([this, World, GameMode, MusicComponent, ComponentProperty]()
			{
				TestEqual(TEXT("The audio device stops music after the fade"), MusicComponent->GetPlayState(), EAudioComponentPlayState::Stopped);
				GameMode->Destroy();
				TestNull(TEXT("Match teardown clears the music reference"), ComponentProperty->GetObjectPropertyValue_InContainer(GameMode));
				TestFalse(TEXT("Match teardown unregisters the music component"), MusicComponent->IsRegistered());
				DestroyGameWorld(World);
			}, 1.0f));
		}
	}
	return true;
}

#endif
