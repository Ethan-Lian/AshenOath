# Boss AI

实现：[Decision Component](../../Source/AshenOath/Private/AI/AshenOathBossDecisionComponent.cpp)、[StateTree Tasks](../../Source/AshenOath/Private/AI/Tasks/AshenOathBossCombatTasks.cpp)、[Boss Character](../../Source/AshenOath/Private/Characters/AshenOathBossCharacter.cpp)。招式执行见 [战斗动作](combat-actions.md)。

## 决策循环

StateTree 编排 Decide、Approach、Observe、Attack、Recovery。Wait 进入 Observe，接近后重新决策，攻击后进入 Recovery。

Decision Component 保存 Snapshot、待执行排名、随机流、冷却、最近两次出招和 Pressure，不启用后台 Tick。Approach / Observe 按配置间隔重新决策，Observe 同时有限速转向。

1. 需要喘息时直接 Wait。
2. 拍下目标距离、朝向和 GAS 可用性，排除距离不合格或冷却中的招式。
3. 按基础分减距离、朝向和近期使用惩罚排序，同分由种子随机流打破。
4. 有候选就攻击；无候选且在近战范围外则接近，否则等待。

近战三招要求水平中心距离 ≤ MeleeRange；DashSwing 要求距离 > DashMinStartRange、≤ DashMaxStartRange，且超过 ActionData.StopDistance。距离与时长以 StateTree / ActionData 资产配置为准。

## Pressure

Pressure 控制是否继续进攻，不参与招式评分。

- 初始为 0；自主攻击请求返回 Running / Succeeded 后增加该招 PressureCost，整套连击只计一次。
- 达到 1 后锁存喘息，后续 Decide 必须 Wait，不取消已受理攻击。
- 只有 Observe 验证目标后释放 Pressure，降到 ResumePressure 才解除锁存；攻击、接近和 Recovery 不释放。
- 请求被拒绝或同步失败不记账；已受理后中断不退款。
- BeginPlay 初始化预算、随机流、冷却和历史；死亡或退出清除待执行决定并停树。

## 请求交接

执行前复核目标和距离，经 Character 请求 GAS。Character 在激活前记下归属；同步结束直接随返回值交付，仍在运行才由 Attack Task 订阅结束事件。

Task 保存本次攻击请求句柄，只接受匹配的结束通知；退出先解除监听，再按句柄取消自己的攻击。SpecHandle 不承担这层请求身份。

StateTree 显式启动，入口演出结束才开放；初始化和退出顺序见 [GAS](ability-system.md)，战斗开始条件见 [对局流程](match-flow.md)。

## 踩坑/变更

- 只有 Rejected 才尝试排名中的下一招，同步 Failed 不继续出招。
- 冷却从请求受理时开始；历史放在随 Boss 存续的组件里，不能随 StateTree Task 重建而丢失。
