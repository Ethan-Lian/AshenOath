#include "Feedback/AshenOathCombatFeedbackComponent.h"

#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Particles/ParticleSystemComponent.h"
#include "Damage/CombatDamageComponent.h"
#include "Death/CombatDeathComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogAshenOathVocals, Log, All);

UAshenOathCombatFeedbackComponent::UAshenOathCombatFeedbackComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UAshenOathCombatFeedbackComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		return;
	}

	if (UCombatDamageComponent* Damage = Owner->FindComponentByClass<UCombatDamageComponent>())
	{
		BoundDamageComponent = Damage;
		DamageResolvedHandle = Damage->OnDamageResolved().AddUObject(
			this, &UAshenOathCombatFeedbackComponent::HandleDamageResolved
		);
	}

	if (UCombatDeathComponent* Death = Owner->FindComponentByClass<UCombatDeathComponent>())
	{
		BoundDeathComponent = Death;
		DeathStartedHandle = Death->OnDeathStarted().AddUObject(
			this, &UAshenOathCombatFeedbackComponent::HandleDeathStarted
		);
		DeathFinishedHandle = Death->OnDeathPresentationFinished().AddUObject(
			this, &UAshenOathCombatFeedbackComponent::HandleDeathAnimationFinished);
	}
}

void UAshenOathCombatFeedbackComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DeathRagdollMesh.Reset();
	bCombatEnded = true;
	ActiveVocalActionId = 0;
	StopActiveVocal();
	SetComponentTickEnabled(false);
	StopDeathEffects();
	DeathMaterials.Reset();

	if (UCombatDamageComponent* Damage = BoundDamageComponent.Get())
	{
		if (DamageResolvedHandle.IsValid())
		{
			Damage->OnDamageResolved().Remove(DamageResolvedHandle);
		}
	}
	DamageResolvedHandle.Reset();
	BoundDamageComponent.Reset();

	if (UCombatDeathComponent* Death = BoundDeathComponent.Get())
	{
		if (DeathStartedHandle.IsValid())
		{
			Death->OnDeathStarted().Remove(DeathStartedHandle);
		}
		Death->OnDeathPresentationFinished().Remove(DeathFinishedHandle);
	}
	DeathStartedHandle.Reset();
	DeathFinishedHandle.Reset();
	BoundDeathComponent.Reset();

	Super::EndPlay(EndPlayReason);
}

uint32 UAshenOathCombatFeedbackComponent::BeginVocalAction()
{
	if (bCombatEnded || bDeathVocalRequested)
	{
		return 0;
	}

	// Zero is reserved for requests that do not belong to an action.
	do
	{
		++NextVocalActionId;
	} while (NextVocalActionId == 0);
	ActiveVocalActionId = NextVocalActionId;
	bActionVocalPlayed = false;
	bFinisherVocalPlayed = false;
	return ActiveVocalActionId;
}

void UAshenOathCombatFeedbackComponent::EndVocalAction(const uint32 ActionId, const bool bWasCancelled)
{
	if (ActionId == 0 || ActionId != ActiveVocalActionId)
	{
		return;
	}

	ActiveVocalActionId = 0;
	if (bWasCancelled && PlayingVocalActionId == ActionId)
	{
		StopActiveVocal();
	}
}

bool UAshenOathCombatFeedbackComponent::RequestActionVocal(
	const uint32 ActionId, const EAshenOathVocalType Type, USoundBase* Sound)
{
	const FAshenOathVocalRule* Rule = VocalRules.Find(Type);
	const UWorld* World = GetWorld();
	const UCombatDeathComponent* Death = BoundDeathComponent.Get();
	const bool bHeavyFinisher = Type == EAshenOathVocalType::HeavyComboFinisher;
	if (bCombatEnded || bDeathVocalRequested || (Death && Death->IsDeathStarted()) ||
		ActionId == 0 || ActionId != ActiveVocalActionId ||
		(bHeavyFinisher ? bFinisherVocalPlayed : bActionVocalPlayed) ||
		!World || !Rule || !IsValid(Sound) || Rule->Chance <= 0.0f || Rule->Volume <= 0.0f)
	{
		return false;
	}

	// Heavy finishers retain their own accent even when an earlier light effort played.
	const bool bSignature = Type == EAshenOathVocalType::SpecialAttack;
	if ((!bSignature && !bHeavyFinisher && World->GetTimeSeconds() < NextEffortTime) || FMath::FRand() >= Rule->Chance)
	{
		return false;
	}

	const EVocalPriority Priority = bSignature ? EVocalPriority::Special :
		((bHeavyFinisher || Type == EAshenOathVocalType::HeavyAttack) ? EVocalPriority::Heavy : EVocalPriority::Light);
	const float Pitch = 1.0f + FMath::FRandRange(-Rule->PitchVariation, Rule->PitchVariation);
	if (!PlayVocal(Sound, Rule->Volume, Pitch, Priority, ActionId))
	{
		return false;
	}

	bActionVocalPlayed = true;
	bFinisherVocalPlayed |= bHeavyFinisher;
	NextEffortTime = World->GetTimeSeconds() + EffortCooldown;
	UE_LOG(LogAshenOathVocals, Verbose, TEXT("%s action=%u type=%s played"),
		*GetNameSafe(GetOwner()), ActionId, *UEnum::GetValueAsString(Type));
	return true;
}

bool UAshenOathCombatFeedbackComponent::PlayVocal(
	USoundBase* Sound, const float Volume, const float Pitch, const EVocalPriority Priority, const uint32 ActionId)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->GetRootComponent() || !IsValid(Sound) || Volume <= 0.0f)
	{
		return false;
	}

	const UAudioComponent* Current = ActiveVocal.Get();
	if (Current && Current->IsPlaying() && Priority <= ActiveVocalPriority)
	{
		return false;
	}

	StopActiveVocal();
	UAudioComponent* Audio = UGameplayStatics::SpawnSoundAttached(
		Sound, Owner->GetRootComponent(), NAME_None, FVector::ZeroVector,
		EAttachLocation::KeepRelativeOffset, true, Volume, Pitch, 0.0f, nullptr, VocalConcurrency.Get(), true
	);
	if (!Audio || !Audio->IsPlaying())
	{
		return false;
	}

	ActiveVocal = Audio;
	ActiveVocalPriority = Priority;
	PlayingVocalActionId = ActionId;
	UE_LOG(LogAshenOathVocals, Verbose, TEXT("%s vocal=%s priority=%d action=%u"),
		*GetNameSafe(Owner), *GetNameSafe(Sound), static_cast<int32>(Priority), ActionId);
	return true;
}

void UAshenOathCombatFeedbackComponent::StopActiveVocal()
{
	UAudioComponent* Audio = ActiveVocal.Get();
	ActiveVocal.Reset();
	PlayingVocalActionId = 0;
	if (Audio)
	{
		Audio->Stop();
	}
}

void UAshenOathCombatFeedbackComponent::StopCombatVocals()
{
	bCombatEnded = true;
	ActiveVocalActionId = 0;
	// Character cleanup may run before or after the DeathStarted listener.
	if (ActiveVocalPriority != EVocalPriority::Death)
	{
		StopActiveVocal();
	}
}

void UAshenOathCombatFeedbackComponent::HandleDamageResolved(
	const FCombatDamageAttempt& /*Attempt*/, ECombatDamageResult Result)
{
	AActor* Owner = GetOwner();

	if (!IsValid(Owner) || Result != ECombatDamageResult::Applied)
	{
		return;
	}

	FVector Location;
	FVector Extent;

	Owner->GetActorBounds(true, Location, Extent);

	if (IsValid(ImpactSound))
	{
		UGameplayStatics::PlaySoundAtLocation(this, ImpactSound.Get(), Location, ImpactVolume);
	}

	// Health callbacks can begin death before Applied is broadcast. Keep the
	// contact sound, but do not overlap pain or hit particles with death feedback.
	UCombatDeathComponent* DeathComponent = BoundDeathComponent.Get();
	if (bDeathVocalRequested || (DeathComponent && DeathComponent->IsDeathStarted()))
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!bCombatEnded && World && World->GetTimeSeconds() >= NextPainTime &&
		PlayVocal(HitSound.Get(), PainVolume, 1.0f, EVocalPriority::Pain))
	{
		NextPainTime = World->GetTimeSeconds() + PainCooldown;
	}

	if (IsValid(HitEffect))
	{
		UGameplayStatics::SpawnEmitterAtLocation(this, HitEffect.Get(), Location);
	}
}

void UAshenOathCombatFeedbackComponent::HandleDeathStarted()
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || bDeathVocalRequested)
	{
		return;
	}
	bDeathVocalRequested = true;
	ActiveVocalActionId = 0;
	StopActiveVocal();

	PlayVocal(DeathSound.Get(), DeathVolume, 1.0f, EVocalPriority::Death);
	SetComponentTickEnabled(true);
}

void UAshenOathCombatFeedbackComponent::ConfigureDeathAnimationDrivenBone(const FName BoneName)
{
	DeathAnimationDrivenBone = BoneName;
}

float UAshenOathCombatFeedbackComponent::GetDeathFinishDuration() const
{
	return DeathPoseHoldSeconds + DeathFadeSeconds;
}

void UAshenOathCombatFeedbackComponent::UpdateDeathGroundContact()
{
	USkeletalMeshComponent* Mesh = DeathRagdollMesh.Get();
	if (!Mesh || bDeathGroundPlayed)
	{
		return;
	}
	const FVector BodyLocation = Mesh->GetSocketLocation(TEXT("pelvis"));
	FHitResult Ground;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(DeathGround), false, GetOwner());
	if (!GetWorld()->LineTraceSingleByChannel(Ground, BodyLocation + FVector(0, 0, 40),
		BodyLocation - FVector(0, 0, 450), ECC_Visibility, Query) || Ground.ImpactNormal.Z <= 0.3f)
	{
		return;
	}
	// Check the simulated collision shapes against the floor, rather than relying
	// on hit-event generation by a particular PhysicsAsset.
	FClosestPointOnPhysicsAsset Contact;
	constexpr float ContactToleranceCm = 6.0f;
	if (Mesh->GetClosestPointOnPhysicsAsset(Ground.ImpactPoint, Contact, false) && Contact.Distance <= ContactToleranceCm)
	{
		PlayDeathGroundImpact(Ground.ImpactPoint, Ground.ImpactNormal);
	}
}

void UAshenOathCombatFeedbackComponent::PlayDeathGroundImpact(const FVector& Location, const FVector& Normal)
{
	bDeathGroundPlayed = true;
	UE_LOG(LogAshenOathVocals, Verbose, TEXT("%s death ground impact at %s"), *GetNameSafe(GetOwner()), *Location.ToCompactString());
	if (DeathGroundEffect)
	{
		UParticleSystemComponent* Particle = UGameplayStatics::SpawnEmitterAtLocation(this, DeathGroundEffect,
			Location + Normal * 3.0f, Normal.Rotation() - FRotator(90, 0, 0),
			FVector(DeathGroundScale));
		DeathParticles.Add(Particle);
	}
	if (DeathGroundSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, DeathGroundSound, Location, DeathGroundVolume);
	}
}

void UAshenOathCombatFeedbackComponent::HandleDeathAnimationFinished()
{
	StartDeathRagdoll();
	DeathAnimationFinishedTime = GetWorld()->GetTimeSeconds();
	USkeletalMeshComponent* Mesh = GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
	if (Mesh && DeathFadeSeconds > 0.0f)
	{
		for (int32 Index = 0; Index < Mesh->GetNumMaterials(); ++Index)
		{
			if (DeathDissolveMaterials.IsValidIndex(Index) && DeathDissolveMaterials[Index])
			{
				Mesh->SetMaterial(Index, DeathDissolveMaterials[Index]);
			}
			UMaterialInterface* Material = Mesh->GetMaterial(Index);
			float Value = 0.0f;
			if (!Material)
			{
				continue;
			}
			TMap<FName, float> Emissive;
			for (const FName Parameter : DeathEmissiveParameters)
			{
				if (Material->GetScalarParameterValue(FMaterialParameterInfo(Parameter), Value))
				{
					Emissive.Add(Parameter, Value);
				}
			}
			if (!Emissive.IsEmpty() || (!DeathDissolveParameter.IsNone() &&
				Material->GetScalarParameterValue(FMaterialParameterInfo(DeathDissolveParameter), Value)))
			{
				DeathMaterials.Add(Mesh->CreateDynamicMaterialInstance(Index));
				DeathInitialEmissive.Add(MoveTemp(Emissive));
			}
		}
	}
	SetComponentTickEnabled(true);
}

void UAshenOathCombatFeedbackComponent::StartDeathRagdoll()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	if (!Mesh || !Mesh->GetPhysicsAsset())
	{
		return;
	}

	Character->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DeathRagdollMesh = Mesh;
	Mesh->SetCollisionProfileName(TEXT("Ragdoll"));
	Mesh->SetEnableGravity(true);
	// SetSimulatePhysics also enables physics pose blending on this mesh.
	Mesh->SetSimulatePhysics(true);
	if (!DeathAnimationDrivenBone.IsNone())
	{
		// Telescoping weapon sections must keep their authored alignment instead of
		// separating into independent rigid bodies when the character ragdolls.
		Mesh->bLocalSpaceKinematics = true;
		Mesh->TermBodiesBelow(DeathAnimationDrivenBone);
	}
	Mesh->WakeAllRigidBodies();
}

void UAshenOathCombatFeedbackComponent::TickComponent(
	const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateDeathCamera(DeltaTime);
	UpdateDeathGroundContact();
	if (DeathAnimationFinishedTime < 0.0)
	{
		return;
	}
	const float Elapsed = GetWorld()->GetTimeSeconds() - DeathAnimationFinishedTime;
	if (Elapsed >= DeathPoseHoldSeconds && DeathFadeSeconds > 0.0f)
	{
		if (!bDeathReleasePlayed && DeathEffect)
		{
			bDeathReleasePlayed = true;
			USkeletalMeshComponent* Mesh = GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
			if (Mesh)
			{
				if (UParticleSystemComponent* Particle = UGameplayStatics::SpawnEmitterAttached(DeathEffect, Mesh, TEXT("pelvis")))
				{
					Particle->SetWorldScale3D(FVector(DeathEffectScale));
					DeathParticles.Add(Particle);
				}
			}
		}
		UpdateDeathMaterials(FMath::Clamp((Elapsed - DeathPoseHoldSeconds) / DeathFadeSeconds, 0.0f, 1.0f));
	}
	if (Elapsed >= GetDeathFinishDuration())
	{
		if (USkeletalMeshComponent* Mesh = DeathRagdollMesh.Get())
		{
			Mesh->PutAllRigidBodiesToSleep();
			if (!DeathDissolveParameter.IsNone())
			{
				Mesh->SetVisibility(false);
			}
		}
		StopDeathEffects();
		SetComponentTickEnabled(false);
	}
}

void UAshenOathCombatFeedbackComponent::UpdateDeathMaterials(const float Progress)
{
	for (int32 Index = 0; Index < DeathMaterials.Num(); ++Index)
	{
		UMaterialInstanceDynamic* Material = DeathMaterials[Index];
		if (!DeathDissolveParameter.IsNone())
		{
			Material->SetScalarParameterValue(DeathDissolveParameter, Progress);
		}
		for (const TPair<FName, float>& Emissive : DeathInitialEmissive[Index])
		{
			Material->SetScalarParameterValue(Emissive.Key, Emissive.Value * (1.0f - Progress));
		}
	}
}

void UAshenOathCombatFeedbackComponent::UpdateDeathCamera(const float DeltaTime)
{
	APlayerController* Controller = GetWorld()->GetFirstPlayerController();
	USkeletalMeshComponent* Mesh = GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
	if (!Controller || !Controller->IsLocalController() || !Controller->PlayerCameraManager || !Mesh)
	{
		return;
	}
	const FVector Focus = Mesh->GetSocketLocation(TEXT("pelvis")) + FVector(0, 0, 40);
	if (APawn* Pawn = Controller->GetPawn())
	{
		if (USpringArmComponent* Boom = Pawn->FindComponentByClass<USpringArmComponent>())
		{
			// Move to the shoulder so the surviving player does not obscure the fallen Boss.
			Boom->SocketOffset.Y = FMath::FInterpTo(Boom->SocketOffset.Y, 85.0f, DeltaTime, 1.8f);
		}
	}
	FRotator Rotation = (Focus - Controller->PlayerCameraManager->GetCameraLocation()).Rotation();
	Rotation.Pitch = FMath::Clamp(Rotation.Pitch, -35.0f, -8.0f);
	Rotation.Roll = 0;
	Controller->SetControlRotation(FMath::RInterpTo(Controller->GetControlRotation(), Rotation, DeltaTime, 1.8f));
}

void UAshenOathCombatFeedbackComponent::StopDeathEffects()
{
	for (const TWeakObjectPtr<UParticleSystemComponent>& Particle : DeathParticles)
	{
		if (Particle.IsValid())
		{
			Particle->DeactivateSystem();
		}
	}
	DeathParticles.Reset();
}
