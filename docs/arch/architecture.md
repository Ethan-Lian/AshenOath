# AshenOath 架构

## 模块

依赖方向：`AshenOath → AshenOathCombat`。

| 模块 | 职责 |
|---|---|
| `AshenOath` | 组装角色、输入、GAS Ability、ActionData、Boss AI、镜头、对局和 UI |
| `AshenOathCombat` | 近战检测、防御窗口、目标侧伤害、受击、死亡、目标持有和位移 Task |

Combat 通过参数接收配置，不引用具体角色、项目 DataAsset、项目 Tag 或 UI。

## 架构图

| 图 | 预览 | Excalidraw 源文件 |
| --- | --- | --- |
| 全局系统架构 | [SVG](diagrams/architecture-overview.svg) | [源文件](diagrams/architecture-overview.excalidraw.md) |
| GAS 动作生命周期 | [SVG](diagrams/gas-action-lifecycle.svg) | [源文件](diagrams/gas-action-lifecycle.excalidraw.md) |
| 近战判定与伤害结算 | [SVG](diagrams/melee-hit-and-damage.svg) | [源文件](diagrams/melee-hit-and-damage.excalidraw.md) |
| 闪避与位移 AbilityTask | [SVG](diagrams/dodge-movement-task.svg) | [源文件](diagrams/dodge-movement-task.excalidraw.md) |
| Boss AI 决策与执行 | [SVG](diagrams/boss-ai.svg) | [源文件](diagrams/boss-ai.excalidraw.md) |

## 系统入口

| 关注点 | 文档 |
|---|---|
| ActorInfo 何时有效，初始属性怎样应用，耐力何时恢复 | [GAS 与属性](../systems/ability-system.md) |
| 动作怎样提交费用，Montage 实例怎样隔离旧 Notify，伤害怎样结算 | [战斗动作与伤害](../systems/combat-actions.md) |
| StateTree 与 Utility 怎样选招，Pressure 怎样控制连续进攻，请求怎样取消 | [Boss AI](../systems/boss-ai.md) |
| 演出何时开放战斗，死亡何时显示结算，重试怎样重建 | [对局流程](../systems/match-flow.md) |
