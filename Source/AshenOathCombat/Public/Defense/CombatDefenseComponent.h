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

	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction
	) override;

	bool CanBeginDodgeWindow(
		const FGameplayTagContainer& WindowTags,
		float StartOffsetSeconds,
		float DurationSeconds,
		float PerfectDodgeStartOffsetSeconds = 0.0f,
		float PerfectDodgeDurationSeconds = 0.0f
	) const;

	// Reserves a window without applying tags. Store the returned handle before
	// calling ActivateDodgeWindow so synchronous tag callbacks can clean it up safely.
	// ExecutionStartWorldTime must use the same UWorld clock as damage attempts.
	FCombatDefenseWindowHandle BeginDodgeWindow(
		UObject* Source,
		const FGameplayTagContainer& WindowTags,
		float StartOffsetSeconds,
		float DurationSeconds,
		double ExecutionStartWorldTime,
		float PerfectDodgeStartOffsetSeconds = 0.0f,
		float PerfectDodgeDurationSeconds = 0.0f
	);

	bool ActivateDodgeWindow(FCombatDefenseWindowHandle Handle);

	void EndDodgeWindow(FCombatDefenseWindowHandle Handle);
	// Force-clears the currently owned window during terminal-state cleanup.
	void ResetDefenseState();

	bool OwnsWindow(FCombatDefenseWindowHandle Handle) const;

	bool IsDodgeWindowActiveAt(const FGameplayTag& Tag, double WorldTimeSeconds) const;

	bool TryConsumePerfectDodge(const FGameplayTag& Tag, double WorldTimeSeconds);

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
	double PerfectDodgeWindowStartWorldTime = 0.0;
	double PerfectDodgeWindowEndWorldTime = 0.0;
	bool bPerfectDodgeConsumed = false;

	int32 NextWindowId = 1;
};
