# Boss AI：选招与压力预算

选招回答“使用哪招”，Pressure 回答“是否应该继续进攻”。第一阶段保留四招的距离、朝向、近期使用评分和冷却，不再计算 Drive、后撤、治疗机会或三个意图的竞争分数。

## 职责

- Decision Component 保存 Pressure、需要喘息的锁存状态、随机流、冷却与近期招式历史；无需后台采样 Tick。
- StateTree 使用 Decide、Approach、Observe、Attack、Recovery。Wait 进入 Observe；攻击后沿用既有 Recovery，再决定继续出招还是喘息。
- GAS 唯一拥有一次攻击的执行。Pressure 达到上限不取消已受理攻击；动作结束后才进入喘息。Combat 模块不引用 Boss AI。

## 预算与生命周期

Pressure 初始为 0，上限为 1。自主选招请求返回 Running/Succeeded 后，才按该招的 PressureCost 增加预算消耗；默认 SingleSwing=0.25、Combo=0.5、ChargedSwing=0.5、DashSwing=0.4。一次 Combo 按整套动作记一次，不按命中次数计入。拒绝和同步失败不计入，已受理后被打断不退款。

达到 1 时锁存 `bNeedsBreather`。后续 Decide 必须选择 Wait，距离和换招不能绕过喘息；原攻击仍由 Ability 执行到结束。只有 Observe Task 的 Tick 在验证当前目标后调用 `ReleasePressureWhileObserving`，按每秒 0.35 线性释放。降到 ResumePressure=0.25 才解除锁存，因此默认完整喘息约 2.14 秒，另加重新决策采样延迟。攻击、Recovery 和 Approach 都不释放 Pressure。

Pressure 不参与招式评分。允许进攻时，有合格攻击就按原评分选招；无合格攻击且位于近战范围外就 Approach，否则 Wait。自然等待冷却时也处于 Observe，可释放尚未耗尽的预算。

Approach 与 Observe 每约 0.15 秒重新决策。Approach 保持原移动请求；Observe 有限速度转向并释放 Pressure。死亡与 EndPlay 清除待执行决定并停止树；新角色 BeginPlay 初始化预算、锁存、冷却和历史。Phase2 尚未接入。

## 验证

`AshenOath.AI.BossPressure` 检查真实 GAS 请求的拒绝、受理、取消和压力累积，验证休息锁存与下阈值恢复；同时启动实际 Boss Blueprint 的 StateTree，确认 Attack/Recovery→Observe→再次 Attack。测试中主动取消攻击以隔离视觉动画更新，不作为实机手感验收。

既有 `AshenOath.Combat.Ability.BossSingleSwingLifecycle` 继续验证距离边界、种子复现、请求身份和冷却。StateTree 资产不加入 Git，使用现有 Wait→Observe、Approach 成功→Decide，保留 MeleeRange=250 cm 与 Recovery=1 s。构建、自动化测试、树编译和实机手感分别记录。

2026-10-01：UE 5.8.2 下 AshenOathEditor Development 构建通过；BossPressure 与 BossSingleSwingLifecycle 均通过；测试加载实际 ST_BossCombat，树编译成功。状态循环观察到 SingleSwing→Combo→ChargedSwing 耗尽预算，再进入 Observe，降到下阈值后恢复攻击。实机手感尚待开发者验证。
