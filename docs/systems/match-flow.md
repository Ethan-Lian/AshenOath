# 对局流程

实现：[GameMode](../../Source/AshenOath/Private/Game/AshenOathGameMode.cpp)、[Death](../../Source/AshenOathCombat/Private/Death/CombatDeathComponent.cpp)、[HUD](../../Source/AshenOath/Private/UI/AshenOathHUD.cpp)、[Feedback](../../Source/AshenOath/Private/Feedback/AshenOathCombatFeedbackComponent.cpp)。

## 入场

入口 Trigger 请求 GameMode 播放 Boss 出场 Sequence：关闭 Gameplay 输入、隐藏玩家状态栏，并推入可选 SoundMix。出场声音由 Sequence Audio Track 配置。

Sequence 结束或主动停止后释放 Mix，恢复视角和输入、显示状态栏与 Boss 血条、启动 Boss StateTree。未配置 Sequence 时直接进入战斗。

GameMode 在战斗启动后持有一份可选的 2D 战斗音乐组件并淡入。循环和 SoundClass 由声音资产配置；结算时立即淡出，退出时停止并释放。

## 死亡与结算

1. Death 观察最终 Health，首次归零先建立不可逆死亡状态并添加 Dead Tag，再广播 DeathStarted。
2. 宿主清理动作、检测、防御、受击、恢复、AI 和移动，向 GameMode 报告死亡；随后 Death 播放死亡 Montage。
3. 首个有效死亡报告锁定 Victory / Defeat，关闭输入并停止存活方战斗行为；世界继续更新。
4. 所属死亡 Montage 结束或中断后广播一次 DeathPresentationFinished，Character 转发给 GameMode。
5. GameMode 只接受决定本局结果的角色通知，等待 DeathPresentationHoldSeconds 后广播 MatchPresentationReady；HUD 此时显示结算并开放鼠标与 Retry。

MatchOutcomeChanged 即时报告胜负，不直接触发结算 UI。HUD 初始化时也查询展示是否已就绪，避免晚订阅漏掉事件。

Retry 经 PlayerController 请求 GameMode，结算展示就绪后重新加载当前关卡；角色和局内状态随关卡重建。

## 反馈

Feedback 监听 Damage / Death：非致命 Applied 在 Bounds 中心播放命中声音与粒子，不要求受击资格；进入死亡后由 DeathStarted 播放死亡反馈。

Feedback 不拥有死亡 Montage 或结算计时；通用 Death 不引用角色、GameMode 或 UI。伤害与受击规则见 [战斗动作](combat-actions.md)。

## 踩坑/变更

- 死亡 Montage 缺失或播放失败，也要在 DeathStarted 广播后报告表现结束，不能卡住结算。
- 演出中结算或退出同样释放 Sequence / SoundMix；EndPlay 清除结算计时器与音乐。
