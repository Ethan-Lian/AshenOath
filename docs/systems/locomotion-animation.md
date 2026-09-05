# 基础移动动画数据

`UAshenOathCharacterAnimInstance` 将角色的实际移动结果转换为四项表现数据。C++ 定义语义和失效处理，AnimBP/Blend Space 负责具体资源与混合。

系统关系见 [架构总览](../architecture.md)。

## 数据接口

接口定义见 [头文件](../../Source/AshenOath/Public/Animation/AshenOathCharacterAnimInstance.h)，采集逻辑见 [实现](../../Source/AshenOath/Private/Animation/AshenOathCharacterAnimInstance.cpp)。变量均为本实例的 Transient、BlueprintReadOnly 数据。

| 变量 | 来源与单位 | 当前规则 |
|---|---|---|
| `GroundSpeed` | CharacterMovement 的世界速度，只取 XY，单位 cm/s | `sqrt(Vx² + Vy²)`；不计垂直速度 |
| `bIsMoving` | GroundSpeed | 严格大于 `3.0 cm/s` 时为 true |
| `MovementDirection` | 水平速度转换到 Character 局部坐标后的方向，单位度 | 前方 0，右方 +90，左方 -90，后方约 ±180；未移动时归零 |
| `bIsInAir` | CharacterMovement 的 `IsFalling()` | 表示下落移动状态，不是完整的所有离地状态分类 |

## 表现约定

- 速度来源为 CharacterMovement 的实际结果；移动和 Gameplay 动作规则由角色侧系统负责。
- MovementDirection 相对身体朝向计算，不使用镜头坐标系，方向转换不受角色缩放影响。
- 原生数据层不做额外平滑，混合过渡由 AnimBP/Blend Space 配置。

## 生命周期与线程边界

| 入口 | 处理 |
|---|---|
| `NativeInitializeAnimation` | 清零数据并刷新 Owner/Movement 弱引用 |
| `NativeUpdateAnimation` | 检查 Pawn Owner 身份或 Movement 缓存失效；读取当前有效对象，无有效对象则清零 |
| `NativeUninitializeAnimation` | 清除两个缓存并清零数据 |

弱引用不延长角色生命周期；无有效 Owner/Movement 时输出默认值。UObject 读取保留在 `NativeUpdateAnimation` 路径，迁入线程安全更新前需先建立安全的数据访问边界。
