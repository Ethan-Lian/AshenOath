# GAS 与属性系统

当前系统为玩家和 Boss 提供 ASC、生命/耐力属性、初始 GameplayEffect 入口和共享状态 Tag。它尚未形成完整的伤害、死亡或技能执行流程。

设计理由见 [ADR-0001](../adr/0001-character-owned-ability-system.md)，系统关系见 [架构总览](../architecture.md)。

## 所有权与入口

| 入口 | 当前责任 |
|---|---|
| [Player Character](../../Source/AshenOath/Private/Characters/AshenOathPlayerCharacter.cpp) | 构造 ASC/AttributeSet，在 `PossessedBy` 显式刷新 ActorInfo 并尝试初始化属性 |
| [Boss Character](../../Source/AshenOath/Private/Characters/AshenOathBossCharacter.cpp) | 构造 ASC/AttributeSet/StateTreeComponent，协调 GAS 与树的启动、退出 |
| [AttributeSet](../../Source/AshenOath/Private/AbilitySystem/AshenOathAttributeSet.cpp) | 维护 Health/MaxHealth、Stamina/MaxStamina 的范围 |
| [Native Gameplay Tags](../../Source/AshenOath/Private/GameplayTags/AshenOathGameplayTags.cpp) | 注册 `State.Dead`、`State.Invulnerable`、`State.Staggered` |

两个角色通过 `IAbilitySystemInterface` 暴露 ASC，通过只读指针暴露属性集；OwnerActor 与 AvatarActor 均为宿主 Character。

## 初始化与退出

### 玩家

```text
PossessedBy(NewController)
  → 父类建立占有关系
  → InitAbilityActorInfo(this, this)
  → ApplyInitialAttributes()
EndPlay
  → ClearActorInfo()
```

`PossessedBy` 刷新 ActorInfo 中的控制关系。同一实例重新被占有时，已成功应用的初始 Effect 不会重放。当前失去控制时没有专门刷新 ActorInfo，调用方不能依赖此期间缓存的 Controller 代表有效占有关系。

### Boss

```text
BeginPlay
  → InitAbilityActorInfo(this, this)
  → ApplyInitialAttributes()
  → StateTreeComponent.StartLogic()
EndPlay
  → StateTreeComponent.StopLogic()
  → ASC.ClearActorInfo()
```

StateTree 关闭自动启动，由 Boss 显式控制顺序：进入逻辑前建立 GAS 上下文，退出逻辑执行后再清理。当前启动流程不以初始 Effect 成功为前提，也不检查树是否实际运行；有效 Effect 和树资产仍是配置要求。

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
