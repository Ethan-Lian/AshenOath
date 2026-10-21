# GAS 与属性

宿主选择见 [ADR-0001](../adr/0001-character-owned-ability-system.md)。实现：[Player](../../Source/AshenOath/Private/Characters/AshenOathPlayerCharacter.cpp)、[Boss](../../Source/AshenOath/Private/Characters/AshenOathBossCharacter.cpp)、[AttributeSet](../../Source/AshenOath/Private/AbilitySystem/AshenOathAttributeSet.cpp)。

## 初始化与退出

| 入口 | 顺序 |
|---|---|
| 玩家 PossessedBy | 刷新 ActorInfo → 应用初始 Effect → 授予配置的 AbilitySpec；重复占有不重复授予 |
| 玩家 BeginPlay / PossessedBy | 初始属性成功后初始化 Death 观察；不假定两个入口的先后顺序 |
| 玩家 UnPossessed | 解除锁定、取消治疗；当前不专门刷新 ActorInfo |
| 玩家 EndPlay | 清理监听、战斗 Ability、耐力恢复、Spec 句柄和 ActorInfo |
| Boss BeginPlay | 初始化决策状态和 ActorInfo → 绑定结束/死亡事件 → 应用初始 Effect → 初始化 Death → 授予 Ability → 注册 GameMode |
| Boss EndPlay | 停 StateTree → 取消残留 Ability、结清请求 → 解除监听并注销 Boss → ClearActorInfo |

初始 Effect 只在成功后标记完成，同一实例不重放；缺失或失败没有自动重试调度。Boss 初始化失败、已经死亡或缺少必需的单次挥击配置时，不继续注册。

角色通过 IAbilitySystemInterface 暴露 ASC。战斗 Ability 使用 InstancedPerActor、ServerOnly，当前没有网络预测。

## 属性

Health / MaxHealth、Stamina / MaxStamina 使用相同规则：Max 最低为 0，Current 截断到 `[0, Max]`；降低 Max 会压低 Current，提高 Max 不补满。

PreAttributeChange 限制新值，PostAttributeChange 维护 Max 与 Current 的关系，PostGameplayEffectExecute 归整 Effect 执行后的值；三个回调不是固定流水线。

治疗次数和提交行为见 [战斗动作](combat-actions.md)，Health 归零后的处理见 [对局流程](match-flow.md)。

## 耐力恢复

[StaminaRecovery](../../Source/AshenOath/Private/AbilitySystem/AshenOathStaminaRecoveryComponent.cpp) 持有自己的延迟计时器和周期恢复 Effect，恢复周期与数值由 Effect 资产配置。

- 通用 CostEffect 成功应用后重启延迟，不额外检查实际扣除量；无 CostEffect 的动作不通知。
- 重击使用独立蓄力费用，确认实际消耗后才通知。
- 请求被拒绝不影响已有恢复；死亡和退出停止计时器并移除所属 Effect。

## 踩坑/变更

- 取消 Ability 必须早于 ClearActorInfo，否则 Task 清理时可能已经失去动画和 GAS 上下文。
- UnPossessed 期间不能把 ActorInfo 缓存的 Controller 当作有效占有关系。
