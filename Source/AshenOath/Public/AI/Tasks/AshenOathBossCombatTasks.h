#pragma once

#include "CoreMinimal.h"
#include "StateTreeConditionBase.h"
#include "StateTreeTaskBase.h"
#include "Characters/AshenOathBossCharacter.h"
#include "AshenOathBossCombatTasks.generated.h"

class AAIController;
class AActor;
class AAshenOathBossCharacter;

USTRUCT()
struct ASHENOATH_API FAshenOathBossDecideTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0", Units = "cm"))
	float MeleeRange = 300.0f;

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0", Units = "cm"))
	float DashMinStartRange = 500.0f;

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DashProbability = 0.35f;
};

/** Chooses one approach, attack, or wait intent before the shared combat loop branches. */
USTRUCT(meta = (DisplayName = "Boss Decide Combat Intent", Category = "AshenOath|Boss"))
struct ASHENOATH_API FAshenOathBossDecideTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAshenOathBossDecideTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition
	) const override;
};

USTRUCT()
struct ASHENOATH_API FAshenOathBossIntentConditionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Parameter)
	EAshenOathBossCombatIntent ExpectedIntent = EAshenOathBossCombatIntent::Attack;
};

/** Tests the decision retained by the Boss across a StateTree transition. */
USTRUCT(meta = (DisplayName = "Boss Combat Intent Is", Category = "AshenOath|Boss"))
struct ASHENOATH_API FAshenOathBossIntentCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAshenOathBossIntentConditionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};

USTRUCT()
struct ASHENOATH_API FAshenOathBossApproachTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0", Units = "cm"))
	float AttackRange = 300.0f;

	UPROPERTY(Transient)
	TObjectPtr<AAshenOathBossCharacter> Boss;

	UPROPERTY(Transient)
	TObjectPtr<AActor> TargetActor;

	UPROPERTY(Transient)
	TObjectPtr<AAIController> AIController;

	UPROPERTY(Transient)
	bool bOwnsMoveRequest = false;

	UPROPERTY(Transient)
	float EffectiveRange = 0.0f;
};

/** Moves the Boss into its configured first-phase attack range. */
USTRUCT(meta = (DisplayName = "Boss Approach Player", Category = "AshenOath|Boss"))
struct ASHENOATH_API FAshenOathBossApproachTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAshenOathBossApproachTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition
	) const override;

	virtual EStateTreeRunStatus Tick(
		FStateTreeExecutionContext& Context,
		float DeltaTime
	) const override;

	virtual void ExitState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition
	) const override;
};

USTRUCT()
struct ASHENOATH_API FAshenOathBossSingleSwingTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0", Units = "cm"))
	float DashMinRange = 500.0f;

	UPROPERTY(Transient)
	TObjectPtr<AAshenOathBossCharacter> Boss;

	FAshenOathBossAttackRequestHandle RequestHandle;
	FDelegateHandle AttackEndedDelegateHandle;
};

/** Selects a first-phase attack and waits only for its own request to end. */
USTRUCT(meta = (DisplayName = "Boss First-Phase Attack", Category = "AshenOath|Boss"))
struct ASHENOATH_API FAshenOathBossSingleSwingTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAshenOathBossSingleSwingTaskInstanceData;

	FAshenOathBossSingleSwingTask();

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition
	) const override;

	virtual void ExitState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition
	) const override;
};

USTRUCT()
struct ASHENOATH_API FAshenOathBossRecoveryTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0", Units = "s"))
	float Duration = 1.0f;

	UPROPERTY(Transient)
	float ElapsedTime = 0.0f;
};

/** Provides an explicit non-damaging pause between Boss attack requests. */
USTRUCT(meta = (DisplayName = "Boss Recovery", Category = "AshenOath|Boss"))
struct ASHENOATH_API FAshenOathBossRecoveryTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAshenOathBossRecoveryTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition
	) const override;

	virtual EStateTreeRunStatus Tick(
		FStateTreeExecutionContext& Context,
		float DeltaTime
	) const override;
};
