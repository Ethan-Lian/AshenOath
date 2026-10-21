# AshenOath

`AshenOath` 是使用 Unreal Engine 5.8 开发的单人第三人称 Boss 战项目。玩家在封闭竞技场中通过轻击连招、蓄力重击、闪避和治疗与 Boss 战斗。项目重点是 GAS 动作生命周期、可复用的战斗组件，以及 StateTree 与 Utility Scoring 驱动的 Boss AI。

## 当前实现

### 玩家动作

- Enhanced Input 路由移动、镜头、轻击、重击、闪避、治疗和锁定请求；自由移动使用视角坐标，锁定移动使用角色朝向坐标。
- 轻击连招支持点按与长按混合，通过所属 Combo Window 消费输入并推进 Montage Section，整套连招提交一次动作费用。
- 蓄力重击按实际蓄力消耗的耐力计算伤害倍率；至少蓄力 1 秒才可释放，松键、满蓄或耐力耗尽进入统一释放入口，蓄力中可通过闪避取消。释放时按距离、角度和视线条件修正朝向。
- 前后闪避冻结输入方向，通过代码位移与碰撞 Sweep 处理地形、台阶和阻挡；无敌按世界时间窗口判定。锁定时前/侧翻在位移结束后平滑回正，后翻保持朝向。
- 原地治疗初始可用三次；所属 Montage 的治疗 Notify 成功应用 Instant GameplayEffect 后才扣除次数，提交前中断不会回血或消耗次数。
- 战斗开始后可锁定当前 Boss，镜头平滑追踪目标；目标失效、死亡、超出锁定距离或结束战斗时解除锁定。

### 战斗与 Boss AI

- 玩家与 Boss 各自持有 ASC 和 AttributeSet，GAS 管理生命、耐力、动作互斥、费用、伤害和状态 Tag；独立组件管理玩家跨动作的延迟耐力恢复。
- 近战检测绑定 Montage 播放实例和 Notify 窗口，连续扫掠武器并按窗口去重；目标侧统一处理终止状态、闪避无敌、其他来源无敌和伤害 Effect。
- 每个命中窗口独立配置受击资格；受击组件管理短硬直和受击 Montage，死亡组件负责一次性死亡状态与动作清理。
- Boss 当前实现第一阶段四招：单次挥击、连续挥击、蓄力重挥、突进追击后挥击。
- StateTree 编排 Decide、Approach、Observe、Attack、Recovery；Utility Scoring 根据距离、朝向、冷却与近期使用历史选择攻击。
- Pressure 预算在攻击请求受理后累积，耗尽后进入 Observe 喘息，下降至恢复阈值后重新允许进攻；只有观察阶段释放预算。

### 对局与表现

- 入口 Trigger 启动可配置的 Boss 出场 Level Sequence；演出期间关闭 Gameplay 输入并隐藏玩家状态栏，结束后恢复视角、输入并启动 Boss AI。没有配置 Sequence 时直接进入战斗。
- HUD 观察玩家生命、耐力、治疗次数和 Boss 生命；战斗开始后显示 Boss 血条。
- 命中与死亡反馈组件按结果播放配置的声音和粒子；GameMode 管理可选战斗音乐的淡入、胜负淡出和退出清理。
- 首个有效死亡报告锁定胜负；死亡表现结束并经过配置的停留时间后显示结算与 Retry，重试重新加载当前关卡。

## 架构

项目包含两个运行时模块：

| 模块 | 职责 |
|---|---|
| `AshenOath` | 组装输入、玩家与 Boss、GAS Ability 与 ActionData、Boss 决策和 StateTree、镜头、动画、对局、UI 与音效反馈 |
| `AshenOathCombat` | 提供不依赖具体角色类型的近战检测、防御窗口、目标侧伤害、受击、死亡、目标持有和代码位移 Task |

战斗动作由具体 `GameplayAbility` 持有生命周期：

- `UAshenOathMeleeAttackAbility` 统一管理玩家与 Boss 近战的 Montage、Cost、检测会话和清理；具体 Ability 保留连招、蓄力或 Boss 招式的流程状态。
- `UAshenOathDodgeAbility` 协调 Montage、Cost、位移 Task 和闪避窗口。
- `UAshenOathHealAbility` 管理治疗 Montage、动画来源身份和治疗次数提交。
- ActionData 只保存配置；AbilityTask 管理随本次执行结束的异步工作，组件管理可复用算法和跨动作状态。Combat 模块通过参数接收所需配置。

依赖方向保持单向：`AshenOath` 负责组装 `AshenOathCombat`；通用战斗模块不引用具体玩家、Boss 或 UI 类型。

## 环境与运行

- Unreal Engine 5.8，本地版本为 5.8.2；Win64。
- 单人、单 Boss、封闭竞技场；Boss 战斗当前只覆盖第一阶段。
- 当前不包含多人、开放世界、装备、掉落、普通小怪或投射物系统。

此 Git 仓库保存 C++、配置和文档，`.uasset`、`.umap` 及构建产物按 `.gitignore` 排除。**仅克隆源码不能获得完整可运行工程**；打开关卡需要单独提供完整的本地 Content 资源，包括角色 Blueprint、ActionData、GameplayEffect、Montage、StateTree、UI 和地图。

具备完整资源后：

1. 用 UE 5.8 生成项目文件，构建 `AshenOathEditor` 的 Development Editor / Win64 目标。
2. 打开 `AshenOath.uproject`，加载默认地图 `/Game/AshenOath/Maps/GameLevel`，使用配置的 `BP_AshenOathGameMode`。
3. 在 PIE 中进入 Boss 入口 Trigger，完成演出、战斗和结算流程。具体键位以 PlayerController 的 `GameplayMappingContext` 资产为准。

## 文档

- [架构总览](docs/arch/architecture.md)
- [战斗动作与伤害](docs/systems/combat-actions.md)
- [GAS 与属性系统](docs/systems/ability-system.md)
- [Boss AI 与 Pressure 预算](docs/systems/boss-ai.md)
- [玩家控制、自由镜头与锁定](docs/systems/player-control-camera.md)
- [动画移动表现](docs/systems/locomotion-animation.md)
- [ADR：Character 持有 Ability System](docs/adr/0001-character-owned-ability-system.md)
- [ADR：Controller 与 Character 的输入边界](docs/adr/0002-player-input-and-character-boundary.md)
- [ADR：GameplayAbility 拥有动作生命周期](docs/adr/0003-gameplay-ability-owns-combat-action-lifecycle.md)
