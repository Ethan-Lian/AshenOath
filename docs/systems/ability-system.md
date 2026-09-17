# GAS 与属性系统

当前系统为玩家和 Boss 提供 ASC、生命/耐力属性、初始 GameplayEffect 入口和共享状态 Tag。单段近战伤害、动作耐力消耗、延迟恢复和普通闪避无敌已接入 GAS；Health 归零后的完整死亡/胜负流程仍未实现。

设计理由见 [ADR-0001](../adr/0001-character-owned-ability-system.md)，系统关系见 [架构总览](../architecture.md)。

## 所有权与入口

| 入口 | 当前责任 |
|---|---|
| [Player Character](../../Source/AshenOath/Private/Characters/AshenOathPlayerCharacter.cpp) | 构造 ASC/AttributeSet、Defense 与耐力恢复等组件；在 `PossessedBy` 刷新 ActorInfo 并授予 Ability，在 BeginPlay 配置恢复与闪避窗口 Tag 并监听 Dead |
| [Boss Character](../../Source/AshenOath/Private/Characters/AshenOathBossCharacter.cpp) | 构造 ASC/AttributeSet/StateTreeComponent，授予单次挥击 Ability，并协调 GAS 与树的启动、退出 |
| [AttributeSet](../../Source/AshenOath/Private/AbilitySystem/AshenOathAttributeSet.cpp) | 维护 Health/MaxHealth、Stamina/MaxStamina 的范围 |
| [Native Gameplay Tags](../../Source/AshenOath/Private/GameplayTags/AshenOathGameplayTags.cpp) | 注册 `State.Dead`、`State.Invulnerable`、`State.Staggered` |

两个角色通过 `IAbilitySystemInterface` 暴露 ASC，通过只读指针暴露属性集；OwnerActor 与 AvatarActor 均为宿主 Character。

## 初始化与退出

### 玩家

```text
BeginPlay
  → 配置 StaminaRecovery 的恢复 Effect/延迟
  → 配置 CombatDefense 的闪避窗口 Tag
  → 配置 CombatDamage 的 Invulnerable Tag
  → 监听 State.Dead，进入时取消动作并停止恢复
PossessedBy(NewController)
  → 父类建立占有关系
  → InitAbilityActorInfo(this, this)
  → ApplyInitialAttributes()
  → 按 SourceObject 授予轻击、前闪与后闪 AbilitySpec（重复占有不重复授予）
EndPlay
  → 解除 Dead 监听
  → 清理动作、窗口和恢复
  → ClearActorInfo()
```

`PossessedBy` 刷新 ActorInfo 中的控制关系。同一实例重新被占有时，已成功应用的初始 Effect 不会重放。当前失去控制时没有专门刷新 ActorInfo，调用方不能依赖此期间缓存的 Controller 代表有效占有关系。

### Boss

```text
BeginPlay
  → InitAbilityActorInfo(this, this)
  → ApplyInitialAttributes()
  → 授予配置了 ActionData 的 Boss 单次挥击 Ability
  → 注册到 GameMode
  → StateTreeComponent.StartLogic()
EndPlay
  → StateTreeComponent.StopLogic()
  → 解除 Ability 结束监听并取消残留战斗 Ability
  → ASC.ClearActorInfo()
```

StateTree 关闭自动启动，由 Boss 显式控制顺序：初始属性、单次挥击配置和 GameMode 注册成功后才启动；退出时先停树，使活动 Task 能解除监听并取消自己等待的挥击，再清理 ASC。Boss 将 `TryActivateAbility` 的接受结果返回给 Task，并只转发该 AbilitySpec 的结束结果，避免其他 Ability 或旧通知结束当前 Task。有效初始 Effect、ActionData 和树资产都是运行配置要求。

### 初始 Effect 的应用

初始属性由角色配置的 GameplayEffect 应用，仅在应用成功后标记初始化完成。同一实例不重复初始化；未配置或应用失败时不会标记成功，也没有自动重试调度。

## 属性规则

生命和耐力采用同样规则：

| 情况 | 当前处理 |
|---|---|
| Max 小于 0 | 收敛到 0 |
| Current 小于 0 或大于 Max | 截断到 `[0, Max]` |
| Max 降低到 Current 以下 | 同步降低 Current |
| Max 提高 | 保留 Current，不补满，也不按百分比调整 |

三个回调分别处理不同修改入口，不保证每次修改均依次触发：

- `PreAttributeChange`：限制即将生效的属性值。
- `PostAttributeChange`：Max 变化后维护 Current 与 Max 的跨属性关系。
- `PostGameplayEffectExecute`：Effect 执行后读取已提交值并归整相关属性。

`PostGameplayEffectExecute` 对应 Effect 执行导致的 BaseValue 修改，不覆盖所有持续效果应用。当前属性回归集中在 Instant Effect，持续与叠加效果需另行验证。

玩家耐力恢复使用一个无限期周期 Effect，每 0.1 秒增加 2 点 Stamina，并继续经过 AttributeSet 的 `[0, MaxStamina]` 约束。`UAshenOathStaminaRecoveryComponent` 只持有自己的延迟计时器和恢复 Effect；任一 Combat Ability 成功提交非零 Cost 后通知它重启恢复，拒绝动作不会改动当前恢复状态。详见 [战斗动作与伤害](combat-actions.md)。
