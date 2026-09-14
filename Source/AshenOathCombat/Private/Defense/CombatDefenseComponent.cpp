#include "Defense/CombatDefenseComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"

UCombatDefenseComponent::UCombatDefenseComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UCombatDefenseComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!CurrentWindow.IsValid())
	{
		SetComponentTickEnabled(false);
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		EndDodgeWindow(CurrentWindow);
		return;
	}

	UpdateWindowState(World->GetTimeSeconds());
}

bool UCombatDefenseComponent::CanBeginDodgeWindow(
	const FGameplayTagContainer& WindowTags,
	const float StartOffsetSeconds,
	const float DurationSeconds) const
{
	return !CurrentWindow.IsValid() &&
		!WindowTags.IsEmpty() &&
		StartOffsetSeconds >= 0.0f &&
		DurationSeconds > KINDA_SMALL_NUMBER &&
		ResolveAbilitySystemComponent() != nullptr;
}

FCombatDefenseWindowHandle UCombatDefenseComponent::BeginDodgeWindow(
	UObject* Source,
	const FGameplayTagContainer& WindowTags,
	const float StartOffsetSeconds,
	const float DurationSeconds,
	const double ExecutionStartWorldTime)
{
	FCombatDefenseWindowHandle StartedWindow;

	if (!IsValid(Source) ||
		!CanBeginDodgeWindow(WindowTags, StartOffsetSeconds, DurationSeconds))
	{
		return StartedWindow;
	}

	StartedWindow.Value = AllocateWindowId();
	CurrentWindow = StartedWindow;
	CurrentWindowSource = Source;
	ConfiguredWindowTags = WindowTags;
	WindowStartWorldTime = ExecutionStartWorldTime + StartOffsetSeconds;
	WindowEndWorldTime = WindowStartWorldTime + DurationSeconds;

	SetComponentTickEnabled(true);

	return StartedWindow;
}

bool UCombatDefenseComponent::ActivateDodgeWindow(
	const FCombatDefenseWindowHandle Handle)
{
	UWorld* World = GetWorld();
	if (!World || !OwnsWindow(Handle))
	{
		return false;
	}

	UpdateWindowState(World->GetTimeSeconds());
	return OwnsWindow(Handle);
}

void UCombatDefenseComponent::EndDodgeWindow(const FCombatDefenseWindowHandle Handle)
{
	if (!Handle.IsValid() || CurrentWindow.Value != Handle.Value)
	{
		return;
	}

	// Revoke ownership before removing loose tags because GAS tag delegates can
	// execute immediately and attempt another cleanup.
	CurrentWindow.Reset();
	CurrentWindowSource.Reset();
	ConfiguredWindowTags.Reset();
	WindowStartWorldTime = 0.0;
	WindowEndWorldTime = 0.0;
	SetComponentTickEnabled(false);
	DeactivateWindowTags();
}

bool UCombatDefenseComponent::OwnsWindow(const FCombatDefenseWindowHandle Handle) const
{
	return Handle.IsValid() && CurrentWindow.Value == Handle.Value;
}

bool UCombatDefenseComponent::IsDodgeWindowActiveAt(
	const FGameplayTag& Tag,
	const double WorldTimeSeconds) const
{
	return CurrentWindow.IsValid() &&
		CurrentWindowSource.IsValid() &&
		Tag.IsValid() &&
		ConfiguredWindowTags.HasTagExact(Tag) &&
		WorldTimeSeconds >= WindowStartWorldTime &&
		WorldTimeSeconds < WindowEndWorldTime;
}

bool UCombatDefenseComponent::IsWindowTagApplied(const FGameplayTag& Tag) const
{
	return Tag.IsValid() && AppliedWindowTags.HasTagExact(Tag);
}

void UCombatDefenseComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	EndDodgeWindow(CurrentWindow);
	Super::EndPlay(EndPlayReason);
}

int32 UCombatDefenseComponent::AllocateWindowId()
{
	const int32 AllocatedId = NextWindowId;
	NextWindowId = NextWindowId == MAX_int32 ? 1 : NextWindowId + 1;
	return AllocatedId;
}

UAbilitySystemComponent* UCombatDefenseComponent::ResolveAbilitySystemComponent() const
{
	const IAbilitySystemInterface* AbilitySystemOwner =
		Cast<IAbilitySystemInterface>(GetOwner());

	return AbilitySystemOwner
		? AbilitySystemOwner->GetAbilitySystemComponent()
		: nullptr;
}

void UCombatDefenseComponent::UpdateWindowState(const double WorldTimeSeconds)
{
	if (WorldTimeSeconds >= WindowStartWorldTime &&
		WorldTimeSeconds < WindowEndWorldTime)
	{
		ActivateWindowTags();
	}
	else if (WorldTimeSeconds >= WindowEndWorldTime)
	{
		DeactivateWindowTags();
		SetComponentTickEnabled(false);
	}
}

void UCombatDefenseComponent::ActivateWindowTags()
{
	if (!CurrentWindow.IsValid() ||
		!AppliedWindowTags.IsEmpty() ||
		ConfiguredWindowTags.IsEmpty())
	{
		return;
	}

	if (UAbilitySystemComponent* AbilitySystemComponent = ResolveAbilitySystemComponent())
	{
		// Record the exact count owned here before broadcasting tag changes.
		AppliedWindowTags = ConfiguredWindowTags;
		AbilitySystemComponent->AddLooseGameplayTags(ConfiguredWindowTags);
	}
}

void UCombatDefenseComponent::DeactivateWindowTags()
{
	if (AppliedWindowTags.IsEmpty())
	{
		return;
	}

	const FGameplayTagContainer TagsToRemove = AppliedWindowTags;
	AppliedWindowTags.Reset();

	if (UAbilitySystemComponent* AbilitySystemComponent = ResolveAbilitySystemComponent())
	{
		AbilitySystemComponent->RemoveLooseGameplayTags(TagsToRemove);
	}
}
