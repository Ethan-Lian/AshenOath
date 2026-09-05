#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "AshenOathPlayerCharacter.generated.h"

class UAshenOathAttributeSet;
class UAbilitySystemComponent;
class UGameplayEffect;
class UCameraComponent;
class USpringArmComponent;



/**
 * Player-side GAS host.
 *
 * The character owns both the AbilitySystemComponent (ASC) and AttributeSet, so
 * the character is both GAS OwnerActor and AvatarActor. This keeps ownership
 * simple while attributes and abilities live only as long as the pawn.
 */
UCLASS()
class ASHENOATH_API AAshenOathPlayerCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AAshenOathPlayerCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	
	const UAshenOathAttributeSet* GetAttributeSet() const;
	
	// Convert 2D movement input from camera space into world-space movement directions, then pass it to CharacterMovement.
	void RequestMove(const FVector2D& MovementIntent, float ReferenceYaw);

protected:
	virtual void EndPlay(
		const EEndPlayReason::Type EndPlayReason
	) override;
	
	virtual void PossessedBy(AController* NewController) override;

private:
	// Constructor-created subobjects share the character's lifetime. UPROPERTY/TObjectPtr
	// keeps them visible to reflection and tracked by Unreal's object/GC system.
	UPROPERTY(
			VisibleAnywhere,
			BlueprintReadOnly,
			Category = "AshenOath|AbilitySystem",
			meta = (AllowPrivateAccess = "true")
		)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(
		VisibleAnywhere,
		Category = "AshenOath|AbilitySystem"
	)
	TObjectPtr<UAshenOathAttributeSet> AttributeSet;
	
	// TSubclassOf exposes a GameplayEffect class asset, not a mutable effect instance.
	UPROPERTY(
		EditDefaultsOnly,
		Category = "AshenOath|AbilitySystem"
	)
	TSubclassOf<UGameplayEffect> InitialAttributesEffect;
	
	UPROPERTY(VisibleAnywhere, Category = "AshenOath|Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = "AshenOath|Camera")
	TObjectPtr<UCameraComponent> FollowCamera;
	
	// PossessedBy may run again after repossession; initial stats are applied only once.
	bool bInitialAttributesApplied = false;

	void ApplyInitialAttributes();
};
