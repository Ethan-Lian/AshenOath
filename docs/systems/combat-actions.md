# 战斗动作与伤害

生命周期分工见 [ADR-0003](../adr/0003-gameplay-ability-owns-combat-action-lifecycle.md)。实现：[Abilities](../../Source/AshenOath/Private/AbilitySystem/Ability)、[Melee](../../Source/AshenOathCombat/Private/Actions/CombatMeleeComponent.cpp)、[Damage](../../Source/AshenOathCombat/Private/Damage/CombatDamageComponent.cpp)。

## 执行

Ability 从 Spec.SourceObject 读取 ActionData。CombatAbility 共享动作互斥与费用检查，死亡或硬直时拒绝激活。

轻击：检查配置/资源 → 播放 Montage 并确认实例 → 建立 Melee 配置快照 → CommitAbility。通用 CostEffect 必须是 Instant；没有 CostEffect 可免费执行，成功扣费后的中断不退款。

Montage 和 Effect 调用可能同步结束 Ability，返回后复核执行与会话。正常结束、失败和取消都进入 EndAbility，先结束 Melee 会话，再由 GAS 清理 Task。

| Ability | 行为 |
|---|---|
| ComboAttack | 每个 Combo Window 接受一次点按或持续按住的输入，推进下一 Section；整套提交一次费用，最后一段不循环 |
| HeavyAttack | 按实际耐力消耗比例计算伤害；松键、满蓄或耐力耗尽走同一释放入口，未满 1 秒则取消，已耗耐力不退 |
| BossSingleSwing | 按配置播放单次挥击 |
| BossCombo | 按 Section 顺序衔接两至三段，整套一个会话、一次费用 |
| BossChargedSwing | 开始扣费，蓄力循环结束后进入挥击，释放时才建 Melee 会话 |
| BossDashSwing | 临时提高移速并追踪玩家，进入 StopDistance 后停止、恢复移速，再建会话挥击；追击段无伤害 |
| Heal | 原地播放 Montage，所属完成 Notify 预留次数 → 应用 Instant Heal Effect → 成功扣次数，失败释放预留 |

轻击锁定辅助由 ComboAttack 在 Montage 与费用提交成功后创建的 ComboTargetAssist Task 执行：第一段起手校正水平朝向，并向起手时的目标位置短冲；最后一段仅在前摇期间以受限角速度追踪目标的实时方位。第二、三段不增加辅助。各段前摇从 Section 起点到该段第一个 Combat Hit Window，缺少命中窗口时不启用辅助；达到命中窗口后停止辅助，不延长 Montage 或动作锁。

辅助只使用当前锁定目标，要求目标存活，并满足 ComboAttackData 的水平距离、身体朝向夹角、高差与视线条件。第一段到两者 Capsule 半径加 ApproachStopGap 的距离即停止接近，最大位移受 ApproachMaxTravelDistance 限制，时长取 ApproachDuration 与剩余前摇时间的较小值；方向和计划距离在起手时冻结，不追加追赶距离。位移复用 CombatMovement 的 Sweep、贴坡和 StepUp。已贴近时仅校正朝向；无目标、超距、遮挡等情况继续普通攻击。解除锁定立即终止本段辅助，目标死亡或失效在下一次辅助更新停止；Ability 结束、中断时清理 Task，并恢复本次接管的移动与 RootMotion 设置。

重击使用 ChargeCostEffect，通用 CostEffect 留空；释放时不再扣第二笔。蓄力中可被闪避取消，已进入释放阶段则不可。释放时只在 Boss 有效、未死亡且满足距离、角度和视线条件时校正一次水平朝向，不要求锁定。

蓄力反馈由 HeavyAttackAbility 提供只读状态：进度与伤害共用实际累计消耗 / 最大蓄力消耗，能否释放独立按最小 1 秒判断。Character 转发变化通知并提供当前快照；PlayerStatusWidget 在订阅时拉取快照，仅蓄力期间显示 ChargeBar 与“蓄力中 / 松开出招”。低耐力起手不重新归一化为满蓄。

Feedback 按本次蓄力句柄持有聚能循环声、长棍粒子和暖色光，强度随进度变化，就绪提示每次仅一次。玩家粒子沿自身局部 X 轴展开，统一挂到 X 轴沿棍身的 FX_Staff_Mid；不直接使用长轴为 Y 的 weapon_r。进入释放时停止持续反馈；Montage 的 Player Heavy Release Notify 在挥击时刻请求短促焰光，核对所属播放实例并去重，破风声沿用原 StaffAudio。Ability 结束清理所属句柄，取消同时停止短反馈；死亡、对局结束和退出经同一清理路径处理，旧句柄不能关闭新蓄力。素材和挂点由 HeavyAttackData 配置。

治疗次数由玩家 Character 持有，初始三次；提交前中断不回血、不扣次数，旧实例和重复 Notify 无效。新角色恢复初始次数。

## 命中窗口

Melee Session Handle 用于精确结束会话；Mesh、AnimInstance、Montage 和播放实例 ID 用于核对 Notify 来源，窗口另核对 Notify 实例 ID。

NotifyBegin 保存配置与采样点，Tick 扫掠采样点前后帧轨迹并补扫相邻点之间的武器主体。目标在提交伤害前加入窗口内集合，防止同步重入造成重复命中。

每个 Combat Hit Window 独立配置受击资格，开窗时保存并随伤害请求传递；不按 Section 名或连段序号推断。

## 闪避

Montage 确认、Cost 成功后，以同一个世界时间启动 Defense 窗口与位移。Defense 先分配句柄交给 Ability，再激活窗口；只增加和移除自己的一层无敌 Tag。

Travel 沿激活时冻结的世界方向执行 Sweep，忽略动画根位移；Walking/NavWalking 下贴坡、StepUp 和贴地，墙体或过高台阶截断位移，受阻距离不补偿。结束/中断恢复 MovementMode 与 RootMotionMode。

前/侧翻朝向位移方向，Travel 结束后用独立 Recovery Task 平滑转回锁定目标的实时方位；仅锁定时选择后翻，后翻保持原朝向，不创建 Recovery。自由状态所有方向输入均选择前滚。Montage 提前结束会清理恢复，不延长动作锁。

## 伤害与受击

Damage 先拒绝已终止的来源/目标，再检查无敌：ASC 存在 Defense 自持层之外的无敌时返回 OtherInvulnerable，否则按 HitTimeSeconds 查询闪避窗口，命中则返回 DodgeInvulnerable；其余经 GAS 应用伤害 Effect。

只广播 Applied 和两类无敌结果，Invalid 不广播。HitReaction 只响应有受击资格的 Applied：建立短硬直 → 取消可中断动作 → 停止移动并播放受击 Montage；结束只清理自身状态。

死亡、反馈和结算见 [对局流程](match-flow.md)，Boss 请求交接见 [Boss AI](boss-ai.md)，耐力恢复见 [GAS](ability-system.md)。

## 踩坑/变更

- SpecHandle 标识授予记录，不能隔离旧动画回调；旧 Montage 的 Notify 要靠播放实例 ID 拒绝。
- Combo Window 必须落在所属 Section 内，相邻窗口留间隔；跨段或重叠会漏掉下一次开窗。
- 闪避 Montage 的尾段要覆盖 FacingRecoveryDuration；StartSection、播放速率或位移时长变化后需一起核对。
