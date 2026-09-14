# ADR-0003：GameplayAbility 拥有战斗动作生命周期

状态：已采用。记录日期：2026-09-14。阶段 B 已迁移单段轻击，阶段 C 已迁移闪避、Defense 与恢复；阶段 D 删除旧路径。

## 背景

原 `UCombatActionComponent` 同时管理动作互斥、Montage、费用、近战、位移、窗口和恢复，形成一套与 GAS Ability 生命周期并行的状态机。轻击接入 GAS 后，如果继续让 ActionComponent 拥有执行，就需要在 AbilitySpec、动作句柄和动画回调之间维持重复身份与清理规则。

## 决定

每个具体 GameplayAbility 唯一拥有本次动作的流程和结束语义；随执行结束的异步工作由 AbilityTask 管理；跨动作或可复用的算法状态由能力组件管理。

单段轻击由 `UAshenOathLightAttackAbility` 协调 `UAbilityTask_PlayMontageAndWait`、Cost 和 `UCombatMeleeComponent`。Melee 独立分配检测会话，保存配置和 Montage 播放实例身份。共享 Notify 直接把 UE 提供的动画来源身份交给 Melee，不查询“当前 Ability”。

闪避由 `UAshenOathDodgeAbility` 协调 Montage、`UAbilityTask_ApplyCombatMovement` 和 `UCombatDefenseComponent`。位移 Task 只拥有本次移动接管，Defense 只拥有窗口时间、来源句柄和自己施加的 Tag；`UAshenOathStaminaRecoveryComponent` 独立拥有跨动作的延迟与恢复 Effect。轻击和闪避通过薄 `UAshenOathCombatAbility` 共享费用事务，不共享各自流程。

`FGameplayAbilitySpecHandle` 仅标识授予记录，不代表某次执行。旧回调隔离使用 UE 的 Montage 实例 ID；目标侧伤害请求暂时保留独立 AttackInstanceId/HitId 契约。

## 取舍

| 结果 | 影响 |
|---|---|
| 复用 GAS 的激活、互斥、取消和 Task 清理 | 不再维护完整的自定义动作状态机；具体 Ability 必须严格处理同步取消和每条结束路径 |
| Melee 会话与动画播放实例绑定 | 旧 Notify 无法污染下一次攻击；动画适配层需要读取 UE 5.8 的 Notify/Montage 上下文 |
| ActionData 继续作为配置单一来源 | 迁移期不重复配置 Cost/伤害；Ability 仍需从 Spec SourceObject 解析并验证该数据 |
| 分阶段保留旧 ActionComponent | 阶段 C 完成后只作为未启用的对照实现；阶段 D 仍需删除类、组件和失效接口并验证资产/Cook |
| 当前采用 `ServerOnly` | 符合现有权威单机范围；未来实现联机预测时必须重新设计费用、命中和回滚契约 |

模块依赖保持 `AshenOath → AshenOathCombat`。Combat 组件不引用具体角色、项目 Tag 或 UI。
