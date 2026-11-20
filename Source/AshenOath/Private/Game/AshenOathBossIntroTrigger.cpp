#include "Game/AshenOathBossIntroTrigger.h"

#include "Characters/AshenOathPlayerCharacter.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Game/AshenOathGameMode.h"

AAshenOathBossIntroTrigger::AAshenOathBossIntroTrigger()
{
	TriggerVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerVolume"));
	RootComponent = TriggerVolume;
	TriggerVolume->SetBoxExtent(FVector(100.0f, 250.0f, 150.0f));
	TriggerVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerVolume->SetGenerateOverlapEvents(true);
}

void AAshenOathBossIntroTrigger::BeginPlay()
{
	Super::BeginPlay();
	TriggerVolume->OnComponentBeginOverlap.AddDynamic(
		this, &AAshenOathBossIntroTrigger::HandleEntranceOverlap
	);
}

void AAshenOathBossIntroTrigger::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	TriggerVolume->OnComponentBeginOverlap.RemoveDynamic(
		this, &AAshenOathBossIntroTrigger::HandleEntranceOverlap
	);
	Super::EndPlay(EndPlayReason);
}

void AAshenOathBossIntroTrigger::HandleEntranceOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	AAshenOathPlayerCharacter* EnteringPlayer = Cast<AAshenOathPlayerCharacter>(OtherActor);
	UWorld* World = GetWorld();
	AAshenOathGameMode* GameMode = World ? World->GetAuthGameMode<AAshenOathGameMode>() : nullptr;

	if (EnteringPlayer && GameMode && GameMode->TryStartBossIntro(EnteringPlayer))
	{
		TriggerVolume->SetGenerateOverlapEvents(false);
	}
}
