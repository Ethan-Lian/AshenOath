# 战斗动作与伤害

当前实现由 GameplayAbility 驱动玩家轻击连招、蓄力重击、前/后闪避和三次治疗，以及 Boss 第一阶段的单次挥击、连击、蓄力重挥和突进后挥击，并已接入普通/完美闪避判定、基础受击与死亡清理。游戏模块的 `UAshenOathActionData` 只保存配置，不保存执行状态；旧 `UCombatActionComponent` 及其执行句柄已经删除。

## 职责边界

| 对象 | 当前职责 |
|---|---|
| `UAshenOathCombatAbility` | 共享动作互斥、ActionData Cost 检查/应用和成功消耗通知；不包含攻击或闪避分支 |
| `UAshenOathMeleeAttackAbility` | 玩家与 Boss 近战 Ability 的抽象执行层；拥有 Montage Task、动画来源身份、Cost 提交、Melee 会话和统一结束流程，不包含连招、蓄力或 Boss 选招状态 |
| `UAshenOathComboAttackAbility` | 直接承载轻击连招；拥有 Combo Window、重复输入消费和 Montage Section 推进 |
| `UAshenOathHeavyAttackAbility` | 与 Combo 并列继承 MeleeAttack；拥有按下、蓄力消耗、释放、伤害倍率和自动瞄准 |
| `UAshenOathDodgeAbility` | 拥有一次闪避的方向/朝向策略快照、Montage、Cost、Travel/Recovery Task、Defense 窗口和统一结束流程 |
| `UAshenOathHealAbility` | 原地播放 Cast Montage，只接受所属实例的完成 Notify；成功应用 Heal Effect 后才消耗一次治疗次数 |
| `UAshenOathBossSingleSwingAbility` | 校验单次挥击起始 Section 并启动近战执行；复用 MeleeAttack 的 Montage、Cost、会话和清理流程，不决定接近、恢复或下一招 |
| `UAshenOathBossComboAbility` | 按 ActionData 的 Section 顺序自动衔接两至三段挥击；整套只建立一次 Melee 会话和一次 Cost |
| `UAshenOathBossChargedSwingAbility` | 前摇进入循环，按配置时间释放重挥；开始时提交 Cost，释放时才建立 Melee 会话 |
| `UAshenOathBossDashSwingAbility` | 循环播放原地跑段，以临时加速的 CharacterMovement 追踪玩家；进入停止距离后切入挥击并建立 Melee 会话，追击段无伤害窗口 |
| `UAbilityTask_BossDashChase` | 逐帧将移动输入指向玩家并拥有本次移速覆盖；到达、受阻或取消时停止自己的移动并恢复原移速 |
| `UAbilityTask_ApplyCombatMovement` | 按动作时间执行 Sweep 位移，接管并恢复 MovementMode/RootMotionMode |
| `UAshenOathAbilityTask_RecoverFacing` | 在闪避末段按目标 Actor 的实时位置平滑恢复锁定朝向；目标失效或 Ability 结束时随即清理 |
| `UCombatDefenseComponent` | 保存普通/完美闪避窗口、来源句柄和单次消费状态，仅增加/移除自己持有的窗口 Tag |
| `UAshenOathStaminaRecoveryComponent` | 保存跨动作的延迟计时器和恢复 Effect；成功消耗重启，拒绝请求不触碰 |
| `UCombatMeleeComponent` | 保存本次攻击的配置与动画来源快照；验证 Notify 来源，连续扫掠并在窗口内去重 |
| `UAnimNotifyState_CombatHitWindow` | 从 UE 动画回调提取 Mesh、动画来源、Montage 实例 ID 和 Notify 实例 ID；不保存战斗状态 |
| `UCombatDamageComponent` | 在目标侧拒绝终止目标；按命中时刻区分普通/完美闪避和其他无敌，经 GAS 应用伤害 Effect 并广播解析结果 |
| `UCombatHitReactionComponent` | 监听目标的 Applied 结果，取消可中断动作，拥有短硬直 Tag、计时器和基础受击 Montage |
| `UCombatDeathComponent` | 观察注入的生命属性，拥有一次性终止 Tag、死亡事件和基础死亡 Montage |
| `UAshenOathActionData` | 所有动作共享的 Montage、播放速率、可选起始 Section 和 Cost Effect |
| `UAshenOathMeleeActionData` | 近战伤害 Effect、完美闪避触发资格和扫掠参数；Boss 单次挥击直接使用此类型 |
| `UAshenOathComboAttackData` / `UAshenOathHeavyAttackData` | 在项目模块中分别扩展连招段落与重击蓄力配置，继承近战数据 |
| `UAshenOathDodgeActionData` | 闪避位移、普通与完美窗口配置 |

`AshenOath` 游戏模块拥有动作 DataAsset、角色与 Ability，并把配置转换为 Combat 调用参数；`AshenOathCombat` 只依赖引擎和 GAS 公共接口，不引用项目 DataAsset、玩家、Boss、项目 AttributeSet、原生 Tag 或 UI。

治疗次数由玩家 Character 持有，初始为三次；新 Character 随重试关卡重新创建。Heal Ability 在整个执行期间持有 `State.MovementLocked`，由 Character 立即停止现有移动并拒绝新移动输入。所属 Montage 的完成 Notify 才能尝试预留一次使用、应用配置的 Instant Heal Effect，并在应用成功后扣除次数；重复或旧实例 Notify 被拒绝，Notify 前中断不回血、不扣次数。UI 只订阅 Health、Stamina 与剩余次数，不参与激活或扣费判断。正常结束、取消和死亡时 GAS 释放移动锁。

## 轻击启动事务

轻击按以下顺序执行：

1. Character 请求已授予的 AbilitySpec；GAS 检查动作互斥、死亡/硬直 Tag、配置完整性和当前资源。轻击激活后在整个 Ability 生命周期持有 `State.MovementLocked`，由 Character 清除已有速度并拒绝新的移动输入，镜头观察不受影响。
2. Ability 创建并启动 `UAbilityTask_PlayMontageAndWait`。播放失败会同步结束 Ability，此时尚未提交 Cost。
3. GAS 确认该 Ability 正在播放配置的 Montage 后，Ability 读取实际 `FAnimMontageInstance::InstanceID`，让 Melee 建立检测会话并复制伤害、半径和骨骼链。
4. 会话建立成功后调用 `CommitAbility`。Cost 必须是 Instant GameplayEffect；应用成功一次后才允许本次攻击继续。
5. Cost Effect 的同步回调可能导致死亡或取消，因此返回后重新检查 Ability 与 Melee 会话是否仍属于本次执行。

无 Cost 的动作可免费执行。资源不足在播放前拒绝；Montage 或 Cost 应用失败会停止本次执行且不留下 Melee 状态；成功扣费后的中断不退款。`TryActivateAbility` 的返回值只表示 GAS 是否接受请求，不代表整个动画/费用事务已经完成。

当前项目是权威单机执行，没有预测契约；这些战斗 Ability 都使用 `ServerOnly` 执行策略。

连招的每个 `PlayerComboWindow` 必须完整落在对应 Montage Section 内，并与相邻窗口留出间隔。窗口开始时 Ability 根据当前 Section 决定下一段；跨越 Section 边界或彼此重叠会使下一段的开窗被忽略。

Boss 近战复用同一执行基类，由 StateTree 经 Boss Character 的请求身份接口激活。请求若在 `TryActivateAbility` 内同步结束，直接返回成功或失败；仍在执行时，Task 再订阅携带请求句柄的结束事件。Task 退出先解除监听，再按句柄取消自己的攻击，因此旧请求不能取消下一招。第一阶段暂按距离选择突进，并轮换近距离三招；复杂 Utility 评分留待后续。

第一阶段在玩家超过 300 cm 时需要接近；只有超过可配置的 500 cm 起冲距离、突进资源可用且上一招不是突进时，才按 `DashProbability` 尝试 DashSwing。未选中突进就正常寻路接近 300 cm。突进开始后，Ability 循环播放不含位移的 `DashSection`；追击 Task 将 `MaxWalkSpeed` 临时乘以 `DashSpeedMultiplier`，每帧按玩家实时位置向 CharacterMovement 提交移动输入并面向玩家。进入 `StopDistance` 后停止移动并恢复原移速，Ability 按玩家当前位置转向、建立 Melee 会话并跳到 `SwingSection`。突进不设最大时长或距离；目标失效、持续受阻与动作取消均结束追击，不会在射程外挥击。

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

| 动作 | 位移 | 位移时长 | 无敌窗口 | 完美闪避窗口 |
|---|---:|---:|---:|---:|
| `DA_Player_Dodge_Fwd` | 450 cm | 0.48 s | 0.10～0.40 s | 0.18～0.28 s |
| `DA_Player_Dodge_Bwd` | 360 cm | 0.42 s | 0.09～0.37 s | 0.16～0.26 s |

角色为前后配置分别持有一个 AbilitySpec，两者复用同一 Dodge Ability 类，并用 Spec 的 SourceObject 指向各自 ActionData。Character 在激活前把输入转换为世界方向，并同时选择朝向策略；侧向、前向和斜向前翻朝位移方向转身，后向输入使用后翻资源并保持当前目标朝向。Ability 激活时立即复制两项参数，后续视角和输入变化不改变本次闪避。

闪避在整个 Ability 生命周期持有 `State.MovementLocked`，阻止自由移动或锁定系统与 Dodge 同时改变身体朝向。动作事务建立后，需要对齐的前/侧翻先进入 Travel：角色朝向冻结的位移方向，代码位移沿同一方向执行；到达 `MovementStartTime + MovementDuration` 后进入 Recovery，独立 Task 从当时朝向开始，以 SmoothStep 沿最短 Yaw 路径逐帧转向锁定目标的实时方位。恢复时长由 Dodge Ability 的 `FacingRecoveryDuration` 配置，默认 `0.4s`，不随位移时长缩短。没有锁定目标时只执行 Travel；后撤翻滚使用 `PreserveCurrentFacing`，始终不创建 Recovery。镜头可继续追踪锁定目标。

Travel 与 Recovery 顺序执行，恢复利用 Montage 的落地尾段；该分界由位移配置决定，不检测动画脚部触地。调整 Montage 的 StartSection、播放速率或位移配置时，应保证实际剩余播放时间覆盖 Recovery。Montage 仍拥有正常结束语义，不为回正延长动作锁定；提前结束或中断会清理未完成的恢复，不强制跳到目标朝向。结束或取消时 GAS 销毁两个 Task 并移除 `State.MovementLocked`，Character 再接回自由移动或锁定朝向控制权。

Montage 播放确认且 Cost 提交成功后，Ability 以同一个世界时间创建 Defense 窗口和位移 Task。Task 在整段执行期间忽略动画根位移，位移段临时切换 MovementMode，通过 `SafeMoveUpdatedComponent` 逐帧 Sweep；墙体只允许碰撞有效的位移量，结束或中断恢复先前模式。

Defense 先分配来源句柄，再由 Ability 保存句柄并激活窗口，避免添加 loose Tag 的同步回调先于所有权建立。窗口开始时只增加自己持有的一份 `State.Invulnerable`，结束时只移除这一份。完美窗口必须是普通窗口的真子集；`TryConsumePerfectDodge` 只有在窗口内首次命中时成功，因此一次闪避最多返回一次 `PerfectDodge`。

Damage 以 `HitTimeSeconds` 查询 Defense 的世界时间范围。只有伤害尝试声明 `bCanTriggerPerfectDodge` 且命中时间落在未消费的完美窗口时，普通无敌拒绝才升级为 `PerfectDodge`；窗口内的后续攻击仍按普通闪避无敌处理。若 ASC 还有额外无敌计数，则返回 `OtherInvulnerable`。来源或目标已有终止 Tag 时，请求直接返回 `Invalid`，不再应用 Effect。

## 受击、死亡与结算边界

CombatDamage 在解析每次请求后同步广播结果。HitReaction 只响应 `Applied`：先拥有短硬直 Tag，再取消带可中断动作 Tag 的 Ability、停止移动并播放配置的受击 Montage；计时结束、死亡或 EndPlay 都只清理自己拥有的 Tag 和计时器。

CombatDeath 由游戏模块注入生命属性和终止 Tag，初始化后监听 ASC 属性变化。生命首次降到 0 时，它先记录不可逆死亡状态并添加自己拥有的一份终止 Tag，然后广播 `DeathStarted`，最后播放死亡 Montage。玩家/Boss 宿主分别在回调中清理自身动作、Melee、Defense、Reaction、恢复、AI 和移动，再把死亡报告给 GameMode；通用 Combat 模块不引用具体角色、GameMode 或 UI。

## 恢复与清理

玩家默认在最后一次成功消耗后等待 1 秒，由独立恢复组件启用蓝图配置的 `GE_Player_StaminaRecovery`：无限期、0.1 秒周期，每周期恢复 2 点耐力。轻击和闪避通过共享 Ability Cost 契约只在 Effect 成功应用后通知恢复组件。新一笔成功消耗会移除旧恢复 Effect 并重新计时；资源不足、播放失败和活动中重触发都不会打断已有恢复。

轻击的所有出口汇入 `EndAbility`：先撤销 Ability 保存的会话句柄，再让 Melee 按稳定快照清理窗口与配置，最后由 GAS 结束 Task/停止 Montage。这样 Montage 停止过程中同步到达的 Notify 或 Task 回调只能看见已失效的会话。

闪避的所有出口同样汇入 `EndAbility`：先撤销 Ability 保存的 Defense 句柄，再让 Defense 清除自己拥有的窗口；随后 GAS 销毁位移和 Montage Task，位移 Task 恢复 MovementMode 与 RootMotionMode。死亡广播后由宿主取消战斗 Ability 并复位各 Combat 组件；退出还会解除属性/伤害监听并最终清理 ASC ActorInfo。
