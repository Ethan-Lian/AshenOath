#include "Actions/CombatActionComponent.h"

#include "Actions/CombatActionData.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"

UCombatActionComponent::UCombatActionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCombatActionComponent::BeginPlay()
{
	Super::BeginPlay();

	CachedCharacter = Cast<ACharacter>(GetOwner());
}

void UCombatActionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    CancelCurrentAction(0.0f);
    CachedCharacter.Reset();
    Super::EndPlay(EndPlayReason);
}

ECombatActionStartResult UCombatActionComponent::TryStartAction(
    const UCombatActionData* ActionData,FCombatActionHandle& OutHandle
)
{
    // OutHandle belongs to the caller and may contain a handle from an earlier request.
    // Reset it first so a rejected request always returns an invalid handle.
    // A successful request will replace it with the new action handle.

    OutHandle.Reset();


    //if current handle has value means combatcomponent handle a action now,reject another action active.
    if(CurrentAction.IsValid())
    {
        return ECombatActionStartResult::RejectedAlreadyActive;
    }

    ACharacter* Character = CachedCharacter.Get();
    
    if (!Character || Character->IsActorBeingDestroyed())
    {
        return ECombatActionStartResult::RejectedInvalidOwner;
    }
    
    if (!ActionData || !ActionData->Montage || ActionData->PlayRate<=0.f)
    {
            return ECombatActionStartResult::RejectedInvalidData;
    }
    
    if (!ActionData->StartSection.IsNone() &&
    ActionData->Montage->GetSectionIndex(
        ActionData->StartSection
    ) == INDEX_NONE)
    {
        return ECombatActionStartResult::RejectedInvalidData;
    }
    
    USkeletalMeshComponent* MeshComponent = Character->GetMesh();
    
    UAnimInstance* AnimInstance = MeshComponent? MeshComponent->GetAnimInstance() : nullptr;
    
    if (!AnimInstance)
    {
        return ECombatActionStartResult::RejectedInvalidAnimation;
    } 

    // Montage_Play returns zero when Unreal cannot create a valid playback
	// instance, so runtime ownership is committed only after it succeeds.
    const float PlayedDuration = AnimInstance->Montage_Play(
        ActionData->Montage,
        ActionData->PlayRate
    );

    if (PlayedDuration <= 0.0f)
    {
        return ECombatActionStartResult::RejectedMontageFailed;
    }
    
    CurrentAction.Value = AllocateActionId();
    
    ActiveAnimInstance = AnimInstance;
    
    ActiveMontage = ActionData->Montage;
    
    if (!ActionData->StartSection.IsNone())
    {
        AnimInstance->Montage_JumpToSection(ActionData->StartSection,ActionData->Montage);
    }
    
    FOnMontageEnded EndDelegate;

    // FOnMontageEnded supplies Montage and bInterrupted. BindUObject stores
	// this execution ID as an additional payload for stale-callback checks.
    EndDelegate.BindUObject(
        this,
        &UCombatActionComponent::HandleMontageEnded,
        CurrentAction.Value
    );
    
    // Register MontageEndedDelegate
    AnimInstance->Montage_SetEndDelegate(EndDelegate,ActionData->Montage);
    
    OutHandle = CurrentAction;
    
    return ECombatActionStartResult::Started;
}

void UCombatActionComponent::CancelCurrentAction(float BlendOutTime)
{
    if (!CurrentAction.IsValid()) return;
    
    FinishAction(CurrentAction.Value,true,BlendOutTime);
}

void UCombatActionComponent::HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted, int32 ExpectedActionId)
{
    FinishAction(ExpectedActionId,false,0.0f);
}

void UCombatActionComponent::FinishAction(int32 ExpectedActionId, bool bStopMontage, float BlendOutTime)
{
    // Only the current action may clear its state.
    // Ignore callbacks whose ID does not match the active action.
    if (!CurrentAction.IsValid() || CurrentAction.Value != ExpectedActionId) return;
    
    UAnimInstance* AnimInstance = ActiveAnimInstance.Get();
    UAnimMontage* AnimMontage = ActiveMontage.Get();
    
    // Clear the component's action state before calling animation functions.
    // Stopping a Montage may trigger callbacks that re-enter this component.
    ActiveMontage = nullptr;
    ActiveAnimInstance.Reset();
    CurrentAction.Reset();
    
    if (!AnimInstance || !AnimMontage) return;
    
    // Explicit cancellation calls Montage_Stop, which may later invoke the end callback.
    // The action state has already been cleared, so remove the callback before stopping the Montage.
    FOnMontageEnded EmptyDelegate;
    
    AnimInstance->Montage_SetEndDelegate(EmptyDelegate,AnimMontage);

    if (bStopMontage && AnimInstance->Montage_IsActive(AnimMontage))
    {
        AnimInstance->Montage_Stop(
            FMath::Max(BlendOutTime, 0.0f),
            AnimMontage);
    }
}

bool UCombatActionComponent::IsActionActive() const
{
    return CurrentAction.IsValid();
}

int32 UCombatActionComponent::AllocateActionId()
{
    const int32 AllocatedId = NextActionId;

    NextActionId = NextActionId == MAX_int32 ? 1 : NextActionId + 1;
    
    return AllocatedId;
}
