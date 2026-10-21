# 移动动画

实现：[CharacterAnimInstance](../../Source/AshenOath/Private/Animation/AshenOathCharacterAnimInstance.cpp)，接口见 [头文件](../../Source/AshenOath/Public/Animation/AshenOathCharacterAnimInstance.h)。

## 数据

| 变量 | 语义 |
|---|---|
| GroundSpeed | CharacterMovement 实际 XY 速度，cm/s |
| bIsMoving | GroundSpeed > 3 cm/s |
| MovementDirection | 实际水平速度相对身体朝向的角度：前 0、右 +90、左 -90、后 ±180；静止归零 |
| bIsInAir | CharacterMovement.IsFalling() |
| bIsLockedOn | Targeting.HasTarget()，无组件时为 false |

数据为本实例的 Transient / BlueprintReadOnly，不做额外平滑；AnimBP / Blend Space 负责混合。AnimInstance 只读表现数据，移动和目标行为见 [玩家控制与镜头](player-control-camera.md)。

## 生命周期

- NativeInitializeAnimation：清零并缓存 Character、Movement、Targeting 弱引用。
- NativeUpdateAnimation：Owner 身份变化或 Movement 缓存失效时刷新；无有效 Owner / Movement 则清零。
- NativeUninitializeAnimation：清除缓存和数据。

## 踩坑/变更

- MovementDirection 用身体坐标，不用镜头坐标，转换时忽略角色缩放。
- IsFalling 只表示下落状态，不能当作完整离地分类。
- UObject 读取在 NativeUpdateAnimation；移入线程安全更新前要先建立数据快照。
