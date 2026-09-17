# 战斗动作与伤害

当前实现由 GameplayAbility 驱动玩家单段轻击、前/后闪避和 Boss 单次挥击。共享 `UCombatActionData` 只保存配置，不保存执行状态；旧 `UCombatActionComponent` 及其执行句柄已经删除。

## 职责边界

| 对象 | 当前职责 |
|---|---|
| `UAshenOathCombatAbility` | 共享动作互斥、ActionData Cost 检查/应用和成功消耗通知；不包含攻击或闪避分支 |
| `UAshenOathLightAttackAbility` | 拥有一次轻击的激活、互斥、Montage Task、Cost 提交、Melee 会话和统一结束流程 |
| `UAshenOathDodgeAbility` | 拥有一次闪避的方向快照、Montage、Cost、位移 Task、Defense 窗口和统一结束流程 |
| `UAshenOathBossSingleSwingAbility` | 拥有一次 Boss 挥击的 Montage、Melee 会话、Cost 提交与统一结束流程；不决定接近、恢复或下一招 |
| `UAbilityTask_ApplyCombatMovement` | 按动作时间执行 Sweep 位移，接管并恢复 MovementMode/RootMotionMode |
| `UCombatDefenseComponent` | 保存闪避窗口的世界时间和来源句柄，仅增加/移除自己持有的窗口 Tag |
| `UAshenOathStaminaRecoveryComponent` | 保存跨动作的延迟计时器和恢复 Effect；成功消耗重启，拒绝请求不触碰 |
| `UCombatMeleeComponent` | 保存本次攻击的配置与动画来源快照；验证 Notify 来源，连续扫掠并在窗口内去重 |
| `UAnimNotifyState_CombatHitWindow` | 从 UE 动画回调提取 Mesh、动画来源、Montage 实例 ID 和 Notify 实例 ID；不保存战斗状态 |
| `UCombatDamageComponent` | 在目标侧校验伤害请求；按命中时刻区分动作拥有的闪避无敌和其他来源无敌，再经 GAS 应用伤害 Effect |
| `UCombatActionData` | Montage、Cost/Damage Effect、扫掠参数，以及闪避位移和 Tag 窗口配置 |

`AshenOath` 游戏模块负责组装角色与 Ability；`AshenOathCombat` 只依赖引擎和 GAS 公共接口，不引用玩家、Boss、项目 AttributeSet、原生 Tag 或 UI。

## 轻击启动事务

轻击按以下顺序执行：

1. Character 请求已授予的 AbilitySpec；GAS 检查动作互斥、死亡/硬直 Tag、配置完整性和当前资源。轻击激活后在整个 Ability 生命周期持有 `State.MovementLocked`，由 Character 清除已有速度并拒绝新的移动输入，镜头观察不受影响。
2. Ability 创建并启动 `UAbilityTask_PlayMontageAndWait`。播放失败会同步结束 Ability，此时尚未提交 Cost。
3. GAS 确认该 Ability 正在播放配置的 Montage 后，Ability 读取实际 `FAnimMontageInstance::InstanceID`，让 Melee 建立检测会话并复制伤害、半径和骨骼链。
4. 会话建立成功后调用 `CommitAbility`。Cost 必须是 Instant GameplayEffect；应用成功一次后才允许本次攻击继续。
5. Cost Effect 的同步回调可能导致死亡或取消，因此返回后重新检查 Ability 与 Melee 会话是否仍属于本次执行。

无 Cost 的动作可免费执行。资源不足在播放前拒绝；Montage 或 Cost 应用失败会停止本次执行且不留下 Melee 状态；成功扣费后的中断不退款。`TryActivateAbility` 的返回值只表示 GAS 是否接受请求，不代表整个动画/费用事务已经完成。

当前项目是权威单机执行，没有预测契约；这些战斗 Ability 都使用 `ServerOnly` 执行策略。

Boss 单次挥击复用同一启动事务，但由 StateTree 经 Boss Character 的窄入口请求。Task 在请求前订阅该 AbilitySpec 的结束事件；请求拒绝立即失败，正常 Montage 完成才成功，取消或中断失败。Task 退出先解除监听，再取消仍活动的挥击，因此停止 StateTree 不会遗留 Melee 会话，也不会让取消回调重入已经退出的 Task。

## Notify 与检测会话

Melee 会话拥有两类身份：

- `FCombatMeleeSessionHandle` 只在 Melee 与本次生命周期拥有者之间传递，用于精确结束配置快照。
- UE 的 Montage 实例 ID 标识真实动画播放。Notify 必须同时匹配会话保存的 Mesh、AnimInstance、Montage 及该实例 ID。

普通 Notify 从 `FAnimNotifyEventReference` 的 `FAnimNotifyMontageInstanceContext` 读取 Montage 实例 ID；Branching Point 则使用 `FBranchingPointNotifyPayload`。Notify State 启用 `NoMergeOnConcurrentPlay`，防止同一动画的并发播放共享 Notify 生命周期。

验证通过的 NotifyBegin 记录 Notify 实例并保存各扫掠点的世界位置；NotifyTick 扫过上一帧到当前帧，并补扫相邻采样点之间的武器主体。目标在提交伤害前加入窗口内集合，因此 GAS 同步回调重入或重复 Tick 都不会对同一目标再次提交。NotifyEnd 只有在 Montage 和 Notify 两重身份仍匹配时才关窗。

旧 Montage 的 Begin/Tick/End 即使晚于下一次攻击到达，也因播放实例 ID 不匹配而被忽略。Ability 结束并不依赖 NotifyEnd：正常完成、取消、中断、死亡和 EndPlay 都会显式结束 Melee 会话。

## 闪避位移与无敌

Controller 保存最近的二维移动意图；按下闪避时 Character 选择前翻或后翻 Data Asset，并按当前控制视角把意图冻结为世界水平方向。无方向输入时沿角色前方闪避。

代码位移按动作相对时间累计目标距离，通过 CharacterMovement 的 Sweep 移动，所以阻挡会截断位移。位移段临时保存并切换 MovementMode，忽略 Montage 根位移；结束或中断时恢复原模式和 AnimInstance 的 RootMotionMode。

当前本地配置为：

| 动作 | 位移 | 位移时长 | 无敌窗口 |
|---|---:|---:|---:|
| `DA_Player_Dodge_Fwd` | 450 cm | 0.48 s | 0.10～0.40 s |
| `DA_Player_Dodge_Bwd` | 360 cm | 0.42 s | 0.09～0.37 s |

角色为前后配置分别持有一个 AbilitySpec，两者复用同一 Dodge Ability 类，并用 Spec 的 SourceObject 指向各自 ActionData。Character 在激活前把输入转换为世界方向，Ability 激活时立即复制，后续视角和输入变化不改变本次闪避。

Montage 播放确认且 Cost 提交成功后，Ability 以同一个世界时间创建 Defense 窗口和位移 Task。Task 在整段执行期间忽略动画根位移，位移段临时切换 MovementMode，通过 `SafeMoveUpdatedComponent` 逐帧 Sweep；墙体只允许碰撞有效的位移量，结束或中断恢复先前模式。

Defense 先分配来源句柄，再由 Ability 保存句柄并激活窗口，避免添加 loose Tag 的同步回调先于所有权建立。窗口开始时只增加自己持有的一份 `State.Invulnerable`，结束时只移除这一份。Damage 以 `HitTimeSeconds` 查询 Defense 的世界时间范围；若 ASC 还有额外无敌计数，则返回 `OtherInvulnerable`。

## 恢复与清理

玩家默认在最后一次成功消耗后等待 1 秒，由独立恢复组件启用无限期、0.1 秒周期的 GAS 恢复 Effect；默认每周期恢复 2 点耐力。轻击和闪避通过共享 Ability Cost 契约只在 Effect 成功应用后通知恢复组件。新一笔成功消耗会移除旧恢复 Effect 并重新计时；资源不足、播放失败和活动中重触发都不会打断已有恢复。

轻击的所有出口汇入 `EndAbility`：先撤销 Ability 保存的会话句柄，再让 Melee 按稳定快照清理窗口与配置，最后由 GAS 结束 Task/停止 Montage。这样 Montage 停止过程中同步到达的 Notify 或 Task 回调只能看见已失效的会话。

闪避的所有出口同样汇入 `EndAbility`：先撤销 Ability 保存的 Defense 句柄，再让 Defense 清除自己拥有的窗口；随后 GAS 销毁位移和 Montage Task，位移 Task 恢复 MovementMode 与 RootMotionMode。死亡与退出先取消战斗 Ability，再停止跨动作恢复，最后清理 ASC ActorInfo。
