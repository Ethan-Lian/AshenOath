# GAS 与属性系统

当前系统为玩家和 Boss 提供 ASC、生命/耐力属性、初始 GameplayEffect 入口和共享状态 Tag。单段近战伤害、动作耐力消耗、延迟恢复、普通/完美闪避、基础受击与 Health 归零后的死亡/胜负/重试流程已接入。

设计理由见 [ADR-0001](../adr/0001-character-owned-ability-system.md)，系统关系见 [架构总览](../arch/architecture.md)。

## 所有权与入口

| 入口 | 当前责任 |
|---|---|
| [Player Character](../../Source/AshenOath/Private/Characters/AshenOathPlayerCharacter.cpp) | 构造 ASC/AttributeSet 与 Combat 组件；刷新 ActorInfo、授予 Ability，注入项目 Tag/属性并响应死亡事件 |
| [Boss Character](../../Source/AshenOath/Private/Characters/AshenOathBossCharacter.cpp) | 构造 ASC/AttributeSet、Combat 与 StateTree 组件，授予已配置的第一阶段近战 Ability，并协调请求身份、GAS、树和死亡的启动/退出 |
| [AttributeSet](../../Source/AshenOath/Private/AbilitySystem/AshenOathAttributeSet.cpp) | 维护 Health/MaxHealth、Stamina/MaxStamina 的范围 |
| [Native Gameplay Tags](../../Source/AshenOath/Private/GameplayTags/AshenOathGameplayTags.cpp) | 注册 `State.Dead`、`State.Invulnerable`、`State.Staggered` |

两个角色通过 `IAbilitySystemInterface` 暴露 ASC，通过只读指针暴露属性集；OwnerActor 与 AvatarActor 均为宿主 Character。

## 初始化与退出

### 玩家

```text
BeginPlay
  → 配置 StaminaRecovery 的恢复 Effect/延迟
  → 向 Defense/Damage/HitReaction/Death 注入项目 Tag 与 Health 属性
  → 绑定 DeathStarted；初始属性就绪后初始化 Death 的 Health 观察
PossessedBy(NewController)
  → 父类建立占有关系
  → InitAbilityActorInfo(this, this)
  → ApplyInitialAttributes()
  → 按 SourceObject 授予轻击、前闪与后闪 AbilitySpec（重复占有不重复授予）
EndPlay
  → 解除 DeathStarted 与 Tag 监听
  → 清理动作、窗口和恢复
  → ClearActorInfo()
```

`PossessedBy` 刷新 ActorInfo 中的控制关系。同一实例重新被占有时，已成功应用的初始 Effect 不会重放。当前失去控制时没有专门刷新 ActorInfo，调用方不能依赖此期间缓存的 Controller 代表有效占有关系。

### Boss

```text
BeginPlay
  → InitAbilityActorInfo(this, this)
  → ApplyInitialAttributes()
  → 授予单次挥击及其他已配置 ActionData 的 Boss 近战 Ability
  → 初始化 Death 的 Health 观察并绑定 DeathStarted
  → 注册到 GameMode
  → StateTreeComponent.StartLogic()
EndPlay
  → StateTreeComponent.StopLogic()
  → 取消残留战斗 Ability、结清请求，再解除 Death/Ability 结束监听
  → ASC.ClearActorInfo()
```

StateTree 关闭自动启动，由 Boss 显式控制顺序：初始属性、必需的单次挥击配置和 GameMode 注册成功后才启动；其余三招仅在各自 ActionData 配置后授予。退出时先停树，使活动 Task 先解除监听并按请求句柄取消自己的攻击，再清理 ASC。Boss 在激活前记录请求归属；同步完成直接随返回值交付，异步结束才按请求句柄广播。轮换历史保存在 Boss 实例，避免 StateTree 离开攻击 State 后重建 Task 数据时重置顺序。

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

Health 的数值约束仍只属于 AttributeSet。`UCombatDeathComponent` 观察最终 Health 值并拥有一次性的存活→死亡转换：先添加注入的 `State.Dead`，再广播同步清理事件。该事件由具体角色消费，因为取消玩家恢复、停止 Boss StateTree、禁用移动和报告 GameMode 都属于游戏组装层，而不是通用属性或 Combat 模块职责。

玩家耐力恢复使用一个无限期周期 Effect，每 0.1 秒增加 2 点 Stamina，并继续经过 AttributeSet 的 `[0, MaxStamina]` 约束。`UAshenOathStaminaRecoveryComponent` 只持有自己的延迟计时器和恢复 Effect；任一 Combat Ability 成功提交非零 Cost 后通知它重启恢复，拒绝动作不会改动当前恢复状态。详见 [战斗动作与伤害](combat-actions.md)。
