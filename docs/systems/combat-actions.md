# 战斗动作与伤害

当前实现覆盖单段轻击与玩家前/后闪避，核心运行时代码位于独立的 `AshenOathCombat` 模块。共享 Data Asset 只保存配置；所有可变状态均由角色组件按一次动作句柄持有。

## 组件边界

| 组件 | 当前职责 |
|---|---|
| `UCombatActionComponent` | 验证并启动 Montage，提交一次 Cost，持有动作句柄；协调代码位移、Tag 窗口、恢复计时和幂等清理 |
| `UCombatMeleeComponent` | 复制本次动作的伤害/扫掠配置；接收带 Notify 实例 ID 的窗口信号，连续扫掠并在窗口内去重 |
| `UCombatDamageComponent` | 在目标侧校验伤害请求；按命中时刻区分动作拥有的闪避无敌和其他来源无敌，再经 GAS 应用伤害 Effect |
| `UCombatActionData` | Montage、Cost/Damage Effect、扫掠参数，以及可选的代码位移和单个 Tag 窗口配置 |

Combat 模块通过 `IAbilitySystemInterface` 使用 ASC，不引用玩家、Boss、AttributeSet 或项目原生 Tag。角色在组装时提供 `State.Invulnerable` 和项目自己的耐力恢复 Effect。

## 启动事务

动作启动顺序为：

1. 拒绝已有动作、无效角色、无效 Montage/Section，以及不完整的位移/窗口配置。
2. 若配置 Cost，要求其为 Instant GameplayEffect；先创建 Spec 并用 GAS 校验资源，不足时不播放动画、不改变状态。
3. Montage 成功播放后分配唯一句柄并安装带句柄的结束回调，再提交一次 Cost。应用失败通过统一清理回滚 Montage 和临时动作状态。
4. Cost 成功后重新启动延迟恢复计时；同步 Effect 回调可能取消动作，因此返回后复核句柄，再初始化位移、窗口和近战快照。

无 Cost 的动作允许不配置 Cost Effect。成功动作被中断时默认不返还消耗。

## 闪避位移与无敌

Controller 保存最近的二维移动意图；按下闪避时 Character 选择前翻或后翻 Data Asset，并按当前控制视角把意图冻结为世界水平方向。无方向输入时沿角色前方闪避。

代码位移按动作相对时间累计目标距离，通过 CharacterMovement 的 Sweep 移动，所以阻挡会截断位移。位移段临时保存并切换 MovementMode，忽略 Montage 根位移；结束或中断时恢复原模式和 AnimInstance 的 RootMotionMode。

当前本地配置为：

| 动作 | 位移 | 位移时长 | 无敌窗口 |
|---|---:|---:|---:|
| `DA_Player_Dodge_Fwd` | 420 cm | 0.48 s | 0.10～0.40 s |
| `DA_Player_Dodge_Bwd` | 360 cm | 0.42 s | 0.09～0.37 s |

窗口开始时 Action 只增加自己持有的一份 `State.Invulnerable` loose Tag 计数，结束时只移除这一份。Damage 以 `HitTimeSeconds` 核对窗口；若另有无敌来源，则返回 `OtherInvulnerable`，不把它误认成闪避无敌。

## 恢复与清理

玩家默认在最后一次成功消耗后等待 1 秒，启用无限期、0.1 秒周期的 GAS 恢复 Effect；默认每周期恢复 2 点耐力。新一笔成功消耗会移除旧恢复 Effect 并重新计时，拒绝请求不会打断恢复。

Montage 正常结束、主动中断、`State.Dead` 加入和 EndPlay 最终汇入动作清理：先关闭近战/Tag 窗口与代码位移，恢复移动和根位移模式，清空句柄及 Montage 回调，再按需停止 Montage。死亡与退出同时停止本组件拥有的恢复计时/Effect，不影响 ASC 上的其他来源。
