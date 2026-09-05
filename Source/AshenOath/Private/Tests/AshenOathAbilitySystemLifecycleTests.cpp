#if WITH_DEV_AUTOMATION_TESTS

#include "Characters/AshenOathBossCharacter.h"
#include "Characters/AshenOathPlayerCharacter.h"

#include "AbilitySystem/AshenOathAttributeSet.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystemComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAshenOathAbilitySystemLifecycleTest,
	"AshenOath.AbilitySystem.Character.Lifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FAshenOathAbilitySystemLifecycleTest::RunTest(const FString& Parameters)
{
	// A transient game world makes BeginPlay and possession run through the same
	// engine lifecycle that populates GAS ActorInfo in production.
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	WorldContext.SetCurrentWorld(World);

	FURL URL;
	World->InitializeActorsForPlay(URL);
	World->BeginPlay();

	AAshenOathPlayerCharacter* Player = World->SpawnActor<AAshenOathPlayerCharacter>();
	UAbilitySystemComponent* PlayerAbilitySystem = Player->GetAbilitySystemComponent();

	TestNotNull(TEXT("Player owns an ability system component"), PlayerAbilitySystem);
	TestNotNull(TEXT("Player owns an attribute set"), Player->GetAttributeSet());
	TestEqual(TEXT("Registered player is the initial GAS owner"), PlayerAbilitySystem->GetOwnerActor(), static_cast<AActor*>(Player));
	TestEqual(TEXT("Registered player is the initial GAS avatar"), PlayerAbilitySystem->GetAvatarActor(), static_cast<AActor*>(Player));
	TestNull(
		TEXT("Unpossessed player ActorInfo has no player controller"),
		Player->GetAttributeSet()->GetActorInfo()->PlayerController.Get()
	);

	APlayerController* PlayerController = World->SpawnActor<APlayerController>();
	// Possess must initialize or refresh ActorInfo; the assertions below guard against
	// stale controller references when the player lifecycle changes.
	PlayerController->Possess(Player);

	TestEqual(TEXT("Possessed player is the GAS owner"), PlayerAbilitySystem->GetOwnerActor(), static_cast<AActor*>(Player));
	TestEqual(TEXT("Possessed player is the GAS avatar"), PlayerAbilitySystem->GetAvatarActor(), static_cast<AActor*>(Player));
	TestEqual(
		TEXT("Possession refreshes the player controller in ActorInfo"),
		Player->GetAttributeSet()->GetActorInfo()->PlayerController.Get(),
		PlayerController
	);
	TestEqual(
		TEXT("Player ASC registered its attribute set"),
		PlayerAbilitySystem->GetSet<UAshenOathAttributeSet>(),
		Player->GetAttributeSet()
	);

	AAshenOathBossCharacter* Boss = World->SpawnActor<AAshenOathBossCharacter>();
	UAbilitySystemComponent* BossAbilitySystem = Boss->GetAbilitySystemComponent();

	TestNotNull(TEXT("Boss owns an ability system component"), BossAbilitySystem);
	TestNotNull(TEXT("Boss owns an attribute set"), Boss->GetAttributeSet());
	TestEqual(TEXT("Boss is its GAS owner after BeginPlay"), BossAbilitySystem->GetOwnerActor(), static_cast<AActor*>(Boss));
	TestEqual(TEXT("Boss is its GAS avatar after BeginPlay"), BossAbilitySystem->GetAvatarActor(), static_cast<AActor*>(Boss));
	TestEqual(
		TEXT("Boss ASC registered its attribute set"),
		BossAbilitySystem->GetSet<UAshenOathAttributeSet>(),
		Boss->GetAttributeSet()
	);

	World->EndPlay(EEndPlayReason::Quit);
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);

	return true;
}

#endif
