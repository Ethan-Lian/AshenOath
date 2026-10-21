# 玩家控制与镜头

职责划分见 [ADR-0002](../adr/0002-player-input-and-character-boundary.md)。实现：[Controller](../../Source/AshenOath/Private/Player/AshenOathPlayerController.cpp)、[Player](../../Source/AshenOath/Private/Characters/AshenOathPlayerCharacter.cpp)、[Targeting](../../Source/AshenOathCombat/Private/Targeting/CombatTargetingComponent.cpp)。

## 输入

Controller 每次取当前 Pawn，检查本地控制和 Gameplay 输入开关后转发。键位由 GameplayMappingContext 资产配置。

| 输入 | 转发 |
|---|---|
| Move | 二维意图与 ControlRotation.Yaw；Completed / Canceled 清零最近意图 |
| Look | 更新 ControlRotation；锁定时拒绝手动 Look |
| Combo / Heavy | Started 转发按下，Completed / Canceled 转发松开 |
| Dodge | 携带最近移动意图 |
| Heal / LockOn | 请求治疗 / 切换锁定 |

SetupInputComponent 按 InputComponent 去重绑定；它与 OnPossess 都检查映射是否就绪。仅本地输入、绑定和预期 Pawn 均有效时注册专用 Context。

关闭 Gameplay 输入、UnPossess 和 EndPlay 清理本 Controller 的映射并转发轻击松开；关闭输入或 UnPossess 同时清零移动意图。重击没有额外松开转发，演出准备、对局结束和 Character 退出通过取消 Ability 清理。

## 移动与朝向

RequestMove 的 X = 右、Y = 前。自由移动使用参考 Yaw，锁定移动使用 Character 的 Forward / Right，再由 CharacterMovement 执行。

无 Controller、销毁中或带 Dead / Staggered / MovementLocked 时拒绝移动。移动锁或硬直首次加入时立即清除输入和速度，并暂停移动系统对身体朝向的驱动。

自由时启用 OrientRotationToMovement；锁定且动作未接管时启用 UseControllerDesiredRotation。死亡、硬直或移动锁期间关闭两者，动作自己的转向见 [战斗动作](combat-actions.md)。

闪避按输入后向锥选择前/后配置，无输入沿角色前方；通常按控制视角换算世界方向。锁定纯侧闪改用目标连线的切线，并按 LockedSideDodgeInwardAngle 向内偏转，目标水平重合时沿原方向。

## 锁定与镜头

已有目标时切换输入解除锁定；没有目标时，仅在战斗已开始、玩家未死亡时尝试锁定 GameMode 当前 Boss。没有扫描、切换目标或遮挡筛选。

Targeting 用弱引用持有目标，检查同一 World、非自身、距离和注入的终止 Tag；Tick 复核，目标销毁/死亡/超距时清除。失去占有、关闭输入、对局结束和退出也解除锁定。

锁定焦点取 Boss Bounds 中心加 0.2 × Extent.Z，从当前镜头位置算视角，经 RInterpTo 写入 ControlRotation。动作接管身体时镜头仍可追踪。

SpringArm 使用 PawnControlRotation 并处理碰撞，Camera 挂接其 Socket，不再单独使用 PawnControlRotation。Character 不直接跟随 Controller 的 Pitch / Yaw / Roll。

## 踩坑/变更

- 映射清理要核对安装时的 Subsystem、PlayerInput 和 Context，旧 Controller 不能移除新输入对象的映射；专用 Context 只设一个管理者。
- 同一 InputComponent 运行时替换 Action 资产不会自动重绑。
- 侧闪向内偏角只保证起步方向，不能保证落点更靠近目标。
