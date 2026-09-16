# AshenOath

`AshenOath` 是一个使用 Unreal Engine 5 开发的单人第三人称 Boss 战项目。项目以小型封闭竞技场为边界，重点探索可维护的动作战斗架构、Boss AI，以及高密度 Gameplay 对象的性能优化。

## 当前实现

- 基于 Enhanced Input 的移动、自由镜头、轻击和前后闪避。
- 由 Gameplay Ability System（GAS）管理生命、耐力、动作消耗、伤害、无敌状态和耐力恢复。
- 单段轻击：Montage 播放、武器扫掠、窗口内命中去重及目标侧伤害判定。
- 前后闪避：方向快照、代码驱动位移、碰撞 Sweep 和时间窗口无敌。
- 玩家与 Boss 各自持有 Ability System Component 和 AttributeSet。
- Boss StateTree 的初始化与退出生命周期。

## 架构

项目包含两个运行时模块：

| 模块 | 职责 |
|---|---|
| `AshenOath` | 组装输入、玩家与 Boss、GAS、动画和具体游戏配置 |
| `AshenOathCombat` | 提供不依赖具体角色类型的近战检测、防御窗口、伤害入口和代码位移 Task |

战斗动作由具体 `GameplayAbility` 持有生命周期：

- `UAshenOathLightAttackAbility` 协调 Montage、Cost 和近战检测会话。
- `UAshenOathDodgeAbility` 协调 Montage、Cost、位移 Task 和闪避窗口。
- `UCombatActionData` 只保存动作配置，不保存运行时执行状态。

依赖方向保持单向：`AshenOath` 负责组装 `AshenOathCombat`；通用战斗模块不引用具体玩家、Boss 或 UI 类型。

## 文档

- [架构总览](docs/architecture.md)
- [战斗动作与伤害](docs/systems/combat-actions.md)
- [GAS 与属性系统](docs/systems/ability-system.md)
- [玩家控制与镜头](docs/systems/player-control-camera.md)
- [动画移动表现](docs/systems/locomotion-animation.md)
- [ADR：GameplayAbility 拥有动作生命周期](docs/adr/0003-gameplay-ability-owns-combat-action-lifecycle.md)

## 环境与范围

- Unreal Engine 5.8
- Win64
- 单人第三人称
- 单 Boss、封闭竞技场
- 不包含多人、开放世界、装备、掉落和普通小怪系统

## 后续方向

- 使用 StateTree 管理 Boss 阶段与宏观状态。
- 通过 Utility Scoring 根据距离、朝向、冷却和玩家行为选择攻击。
- 完善输入缓冲、近战连段和死亡流程。
- 对灵魂投射物进行 Actor、对象池、批处理和并行批处理对比，并使用 Unreal Insights 验证结果。
