# AshenOath 架构

UE 5.8 单人第三人称 Boss 战。当前版本以第一阶段近战为主，包含轻击连招、蓄力重击、闪避、治疗、锁定和死亡重试。

## 模块

依赖方向：`AshenOath → AshenOathCombat`。

| 模块 | 职责 |
|---|---|
| `AshenOath` | 组装角色、输入、GAS Ability、ActionData、Boss AI、镜头、对局和 UI |
| `AshenOathCombat` | 近战检测、防御窗口、目标侧伤害、受击、死亡、目标持有和位移 Task |

Combat 通过参数接收配置，不引用具体角色、项目 DataAsset、项目 Tag 或 UI。

## 决策

- [ADR-0001：Character 持有 ASC](../adr/0001-character-owned-ability-system.md)
- [ADR-0002：Controller 与 Character 的输入边界](../adr/0002-player-input-and-character-boundary.md)
- [ADR-0003：GameplayAbility 拥有动作生命周期](../adr/0003-gameplay-ability-owns-combat-action-lifecycle.md)

## 系统

| 关注点 | 文档 |
|---|---|
| ActorInfo 何时有效，初始属性怎样应用，耐力何时恢复 | [GAS 与属性](../systems/ability-system.md) |
| 动作怎样提交费用，Montage 实例怎样隔离旧 Notify，伤害怎样结算 | [战斗动作与伤害](../systems/combat-actions.md) |
| StateTree 与 Utility 怎样选招，Pressure 怎样控制连续进攻，请求怎样取消 | [Boss AI](../systems/boss-ai.md) |
| 输入映射归谁，移动用什么坐标，锁定怎样控制镜头和朝向 | [玩家控制与镜头](../systems/player-control-camera.md) |
| 实际速度怎样转成动画数据，Owner 换掉后怎样处理 | [移动动画](../systems/locomotion-animation.md) |
| 演出何时开放战斗，死亡何时显示结算，重试怎样重建 | [对局流程](../systems/match-flow.md) |