# ADR-0003：GameplayAbility 拥有战斗动作生命周期

已采用 · 2026-09-14

## 背景

一次动作要协调 Montage、费用、检测和中断。Ability 与组件各管一套完整流程，会重复维护状态与清理规则。

## 决定

具体 GameplayAbility 拥有一次动作；AbilityTask 管随执行结束的异步工作，组件管可复用算法或跨动作状态。ActionData 只存配置。

玩家与 Boss 的近战共用 MeleeAttack 执行基类，具体 Ability 保留连招、蓄力和追击流程。闪避、治疗各自编排。

## 取舍

复用 GAS 的互斥、取消和 Task 清理，不再维护并行动作状态机。代价是每条结束路径都要进入 EndAbility，并处理 Effect 或 Montage 调用中的同步取消。

执行时序与身份隔离见 [战斗动作与伤害](../systems/combat-actions.md)，AI 请求交接见 [Boss AI](../systems/boss-ai.md)。
