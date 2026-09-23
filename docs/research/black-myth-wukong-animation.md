# 《黑神话：悟空》角色动画系统公开资料梳理

> 调研日期：2026-09-22  
> 范围：锁定移动、方向闪避、Motion Matching、Root Motion、动作结束后的朝向恢复。优先采用 Game Science 与 Epic Games 的一手资料。

## 结论

公开资料足以确认：《黑神话：悟空》至少在 2021 年开发阶段，为人形角色的 locomotion 使用了 Motion Matching；其锁定移动不是简单地把普通前进动画朝不同方向旋转，而是专门录制“移动方向变化、面部/身体仍朝固定方向”的动作覆盖，再把当前姿势、速度和未来轨迹一起纳入匹配。

公开资料不足以确认：最终发行版的方向闪避究竟由 Motion Matching、Montage、Root Motion、Motion Warping 或自研逻辑中的哪一种驱动，也没有公开闪避后半段如何回正、朝向恢复曲线如何配置。因此，不能把某一种具体实现称为“《黑神话：悟空》的做法”。

对 AshenOath 当前问题最有价值的原则是：**在锁定状态中，把目标朝向、移动轨迹和未来朝向作为同一段动作的连续约束；不要先将 Actor 瞬间转向侧滚方向，再在 Ability 结束时把朝向控制权硬切回 Lock-on。**

## 已确认事实

### 1. 人形 locomotion 使用 Motion Matching

Game Science 联合创始人、技术负责人招文勇在 Epic 的 Unreal Circle 技术演讲中明确表示，项目的人形动作使用 Motion Matching。选择它的原因包括：降低传统 AnimGraph/状态机的拆分与维护复杂度、提高大量角色动画的生产能力，并覆盖启停、加减速、转向和折返等 locomotion 需求。

来源：[《〈黑神话：悟空〉的 Motion Matching》— 招文勇，虚幻引擎官方账号](https://www.bilibili.com/video/BV1GK4y1S7Zw/)

### 2. 自由移动与锁定移动使用不同的动作设计

演讲明确区分：

- 自由移动：角色面朝移动方向。
- 锁定移动：角色无论向哪个方向移动，面部/身体朝向保持在同一目标方向。

锁定动作数据为此专门录制了环绕、S 形、米字、方块/钻石等路线，要求演员在沿路线移动时始终保持固定朝向。这意味着“移动方向”和“目标朝向”在动画资产层就被同时表达，而非只有一条 forward locomotion 再依靠 ActorRotation 临时纠正。

来源：[《〈黑神话：悟空〉的 Motion Matching》— 招文勇，虚幻引擎官方账号](https://www.bilibili.com/video/BV1GK4y1S7Zw/)

### 3. 匹配同时考虑当前姿势与未来轨迹

演讲介绍的 cost 不只比较速度和骨骼姿势，还加入未来若干帧的位置、朝向、加速度与减速度。名为 `Responsive` 的权重用于调节未来因素的重要程度，以在动作真实性与输入响应之间取舍。实现被封装为 AnimNode，可与 AnimGraph、状态机和叠加动画结合。

这说明其设计目标并不是逐帧把模型强行拧向当前输入，而是从动画数据库中选择一个能够延续当前姿势、同时接近未来轨迹和朝向的片段。

来源：[《〈黑神话：悟空〉的 Motion Matching》— 招文勇，虚幻引擎官方账号](https://www.bilibili.com/video/BV1GK4y1S7Zw/)

### 4. 动画真实性与操作响应存在显式权衡

招文勇指出，这套 locomotion 是动画驱动移动，动画录制速度需要提前规划；真实动作中的急停和折返天然需要时间，与游戏要求的即时响应会冲突。因此，他们从录制敏捷程度与匹配权重两端共同调节，而不是假设“动画更真实”自然就会带来更好的操作感。

来源：[《〈黑神话：悟空〉的 Motion Matching》— 招文勇，虚幻引擎官方账号](https://www.bilibili.com/video/BV1GK4y1S7Zw/)

### 5. 项目后来由 UE4 迁移到 UE5

Game Science 在 Epic 的官方采访中确认，项目开发期间从 UE4 迁移到 UE5。该采访主要讨论 Nanite、Lumen 和工作流，没有披露闪避动画的实现。

来源：[《黑神话：悟空》借助 UE5 抢先体验版视效惊艳全网 — Epic 对 Game Science 的采访](https://www.unrealengine.com/developer-interviews/black-myth-wukong-wows-with-ue5-early-access-visuals?lang=zh)

## 没有公开证据支持的说法

截至本次调研，没有找到 Game Science 或 Epic 的一手资料能够证明：

- 最终发行版的方向闪避由 Motion Matching 直接选择。
- 闪避使用或不使用 Animation Montage。
- 闪避位移明确采用 UE Root Motion、代码位移或二者混合。
- 闪避使用 UE Motion Warping。
- 角色在闪避 recovery 阶段以某条确定曲线重新面向锁定目标。
- 2021 年演讲所示的自研 Motion Matching AnimNode，与 UE5 当前 Pose Search/Motion Matching 实现完全相同。

因此，公开视频可以用于观察最终视觉结果，但仅凭画面不能反推出上述内部实现。

## 对 AshenOath 的可落地推论

以下是基于公开原则和当前症状的工程推论，不是对《黑神话：悟空》源码的复述。

### 当前不流畅的原因

当前侧滚流程把三个方向量分离了：

1. 位移沿锁定视角的 A/D 方向。
2. Forward Dodge 动画的身体正面也被瞬间转到 A/D 方向。
3. Ability 结束后，Lock-on 又立即要求身体面向目标。

侧滚期间角色位置已经绕目标变化，结束时目标往往落在身体左后或右后方，于是朝向所有权切换会制造一次大角度回弹。这个问题不是单纯把回转速度调慢就能根治；根因是动作起点、动作轨迹和动作终点姿态没有共同规划。

### 更接近公开设计原则的优先方案

1. **最佳长期方案：专用锁定方向闪避动画。**
   - A/D 使用 left/right dodge 或可镜像的侧向动画。
   - 位移方向与锁定目标朝向同时存在于动画中。
   - 动画结尾姿势天然回到可继续锁定 locomotion 的朝向。

2. **当前只有 Forward Dodge 时：把一次闪避分为 travel 与 recovery 两个朝向阶段。**
   - travel 前段允许身体偏向闪避方向，让动作与位移一致。
   - recovery 后段在 Montage 窗口内逐渐朝锁定目标回正。
   - 到 `EndAbility()` 时应已接近目标朝向；结束只释放所有权，不再制造大角度瞬转。
   - `S + Space` 继续保持面向目标并后撤。

3. **避免把整个侧滚期间的朝向锁死在最初 A/D 方向。**
   - 锁定目标可能移动，角色也在绕目标改变相对位置。
   - recovery 需要根据更新后的目标方向重新计算期望朝向，但应限制角速度并使用动画可接受的窗口。

### 建议的数据与生命周期

- Character 在请求闪避时计算并冻结 `DodgeTravelDirection`。
- Ability 冻结动作类型和 travel/recovery 时间窗，但锁定目标引用仍由 `UCombatTargetingComponent` 持有。
- travel 阶段：移动任务沿 `DodgeTravelDirection` 位移。
- recovery 阶段：Ability 查询当前锁定目标方向，按受限角速度逐步回正。
- Ability 结束或取消：清理 recovery 更新；随后由常规 Lock-on locomotion 接管。
- Targeting 组件仍只管理目标身份与有效性，不承担具体闪避动画策略，模块依赖无需改变。

## 可借鉴的 Epic 官方通用技术

这些技术可以帮助实现上述原则，但没有证据表明《黑神话：悟空》的闪避一定使用它们。

- [Motion Matching](https://dev.epicgames.com/documentation/zh-cn/unreal-engine/motion-matching-in-unreal-engine)：通过姿势历史与预测轨迹，从数据库选择合适姿势；适合拥有足够动作覆盖时构建完整 locomotion。
- [Pose / Orientation Warping](https://dev.epicgames.com/documentation/unreal-engine/pose-warping-in-unreal-engine)：可以在保持面朝角度的同时调整下半身运动方向，用于补足方向动画覆盖；不能凭空替代一段风格正确的 90° 侧滚动画。
- [Motion Warping](https://dev.epicgames.com/documentation/unreal-engine/motion-warping-in-unreal-engine)：可在 Montage 的指定窗口内调整 Root Motion 平移和旋转，使动作在窗口结束时匹配目标 Transform。
- [Game Animation Sample](https://dev.epicgames.com/documentation/unreal-engine/game-animation-sample-project-in-unreal-engine)：展示预测轨迹、Motion Matching、Orientation Warping、Steering 和 Offset Root Bone 如何组合；其中也明确讨论大角度切换朝向需要专门 transition，而非瞬时旋转。

## 资料边界说明

- 2021 年 Unreal Circle 演讲是本次最直接的一手技术资料，但它早于最终发行版，也早于项目完成 UE5 迁移；它能证明当时的设计和实现，不能自动证明最终版本每个动作沿用相同管线。
- 便于文字检索的[演讲节选转录](https://www.sohu.com/a/457769481_204728)不是一手来源，仅用于辅助定位；本文的核心结论均指向 Epic 官方账号发布的原始演讲。
