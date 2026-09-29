#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AshenOathBossIntroTrigger.generated.h"

class UBoxComponent;
class UPrimitiveComponent;

/** Place at the arena entrance. Only the controlled player can start the intro. */
UCLASS()
class ASHENOATH_API AAshenOathBossIntroTrigger : public AActor
{
	GENERATED_BODY()

public:
	AAshenOathBossIntroTrigger();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleEntranceOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);

	UPROPERTY(VisibleAnywhere, Category = "AshenOath|Intro")
	TObjectPtr<UBoxComponent> TriggerVolume;
};
