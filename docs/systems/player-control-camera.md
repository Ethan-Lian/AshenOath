# 玩家控制与自由镜头

当前系统实现键鼠 Move/Look、相对视角移动和第三人称自由镜头。Controller 处理设备输入与映射归属，Character 处理角色移动约束，CharacterMovement 执行移动。

设计理由见 [ADR-0002](../adr/0002-player-input-and-character-boundary.md)，系统关系见 [架构总览](../architecture.md)。实现见 [PlayerController](../../Source/AshenOath/Private/Player/AshenOathPlayerController.cpp) 与 [Player Character](../../Source/AshenOath/Private/Characters/AshenOathPlayerCharacter.cpp)。

## 入口与运行链

```text
MoveAction (Axis2D / Triggered)
  → PlayerController.HandleMove
  → 当前玩家 Character.RequestMove(二维意图, ControlRotation.Yaw)
  → 世界前/右方向上的 AddMovementInput
  → CharacterMovement

LookAction (Axis2D / Triggered)
  → PlayerController.HandleLook
  → AddYawInput / AddPitchInput
  → ControlRotation
  → SpringArm
  → Camera
```

Controller 的 Move/Look 回调先检查本地控制、对应输入未被忽略及当前 Pawn 有效。每次调用读取当前 Pawn，不长期缓存之前控制的身体。

## 输入绑定与映射生命周期

输入组件就绪和完成占有均会触发映射检查；注册与清理须允许重复调用。

| 生命周期入口 | 项目中的处理 |
|---|---|
| `SetupInputComponent` | 验证 Enhanced Input 组件及资产；Move/Look 均须为 Axis2D；按 InputComponent 去重绑定并检查映射 |
| `OnPossess` | 父类建立控制关系后检查映射；本地玩家使用 GameOnly 输入模式并隐藏光标 |
| `OnUnPossess` / `EndPlay` | 清理本 Controller 注册的映射 |

映射优先级为 `0`，注册遵循以下约定：

- 仅在本地输入对象就绪、当前 InputComponent 已绑定且正在控制预期玩家角色时安装；条件失效则清理旧注册。
- 记录安装时的 Subsystem、PlayerInput 和 Context。记录与当前对象一致且映射存在时不重复安装；不接管没有归属记录的已有映射。
- 移除前核对 Subsystem 仍使用安装时的 PlayerInput，避免旧 Controller 清掉新输入对象的映射；随后重置弱引用记录。

专用 Context 只允许一个管理者；注册记录不提供多系统共享或引用计数。当前也不支持同一 InputComponent 上运行时替换 Move/Look 资产后自动重绑。

## 移动与镜头的坐标关系

`RequestMove` 接收 `X = 右、Y = 前` 的二维移动意图和参考 Yaw，将意图转换为世界水平方向后交给 CharacterMovement。无 Controller、正在销毁或带 `State.Dead` 的角色拒绝新移动请求；该入口不负责立即清零已有速度。

| 旋转对象 | 当前原生配置 | 作用 |
|---|---|---|
| Character | 不直接使用 Controller 的 Pitch/Yaw/Roll | 自由看向不强制身体跟转 |
| CharacterMovement | `bOrientRotationToMovement=true`，`bUseControllerDesiredRotation=false` | 由移动方向驱动身体转向 |
| SpringArm | `bUsePawnControlRotation=true`，开启碰撞检测 | 使用控制视角并处理镜头臂碰撞 |
| Camera | 挂接 SpringArm Socket；不再单独使用 PawnControlRotation | 跟随镜头臂输出 |
