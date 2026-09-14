#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Defense/CombatDefenseTypes.h"
#include "GameplayTagContainer.h"
#include "CombatDefenseComponent.generated.h"

class UAbilitySystemComponent;

/**
 * Owns action-scoped defensive windows on a combatant.
 *
 * The window records world-time bounds independently from its currently
 * applied loose tags. Damage can therefore classify a delayed hit by the time
 * at which it happened, while cleanup removes only the tag count added here.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class ASHENOATHCOMBAT_API UCombatDefenseComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatDefenseComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	bool CanBeginDodgeWindow(const FGameplayTagContainer& WindowTags,
	                         float StartOffsetSeconds,
	                         float DurationSeconds) const;

	// ExecutionStartWorldTime must use the same UWorld clock as damage attempts.
	FCombatDefenseWindowHandle BeginDodgeWindow(
		UObject* Source,
		const FGameplayTagContainer& WindowTags,
		float StartOffsetSeconds,
		float DurationSeconds,
		double ExecutionStartWorldTime
	);
	// Second phase: the caller stores the returned handle before this function
	// can add tags and synchronously trigger cancellation callbacks.
	bool ActivateDodgeWindow(FCombatDefenseWindowHandle Handle);

	void EndDodgeWindow(FCombatDefenseWindowHandle Handle);

	bool OwnsWindow(FCombatDefenseWindowHandle Handle) const;
	bool IsDodgeWindowActiveAt(const FGameplayTag& Tag, double WorldTimeSeconds) const;
	bool IsWindowTagApplied(const FGameplayTag& Tag) const;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	int32 AllocateWindowId();
	UAbilitySystemComponent* ResolveAbilitySystemComponent() const;
	void UpdateWindowState(double WorldTimeSeconds);
	void ActivateWindowTags();
	void DeactivateWindowTags();

	FCombatDefenseWindowHandle CurrentWindow;
	TWeakObjectPtr<UObject> CurrentWindowSource;
	FGameplayTagContainer ConfiguredWindowTags;
	FGameplayTagContainer AppliedWindowTags;
	double WindowStartWorldTime = 0.0;
	double WindowEndWorldTime = 0.0;
	int32 NextWindowId = 1;
};
