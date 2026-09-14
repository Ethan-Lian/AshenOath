# AshenOath

`AshenOath` 是一个使用 Unreal Engine 5 开发的单人第三人称动作游戏项目，目标是在封闭竞技场 Boss 战中，集中呈现闪避、近战节奏、Boss 决策以及高密度投射物的性能优化。

项目主要关注：

- 基于闪避、输入缓冲和近战连段的动作战斗。
- 使用 StateTree 管理 Boss 阶段，并通过 C++ Utility Scoring 动态选择攻击。
- 对高密度灵魂投射物进行 Actor、对象池、批处理和并行批处理性能对比，并使用 Unreal Insights 验证优化结果。

## 技术架构

- GAS：负责 Attribute、GameplayEffect、伤害、无敌和跨系统 GameplayTag。
- GameplayAbility / Combat：Ability 负责具体动作生命周期，Combat 组件分别负责近战检测、闪避窗口、伤害判定和代码位移。
- StateTree：负责 Boss 的阶段、转阶段和死亡等宏观状态。
- Utility Evaluator：根据距离、朝向、冷却、玩家近期行为和重复惩罚选择攻击。
- Projectile Subsystem：负责高密度灵魂投射物的批量模拟与性能测试。

## 项目范围

- Unreal Engine 5.8
- Win64
- 单人第三人称
- 单 Boss、三阶段
- 不实现多人、开放世界、装备、掉落和普通小怪

项目重点是战斗架构、Boss AI 和高密度 Gameplay 对象的性能优化。
