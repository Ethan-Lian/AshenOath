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
4. 死亡 Montage 实例关闭自动混出，播放到末帧或中断后广播一次 DeathPresentationFinished。自然结束时保留末帧，Feedback 接管尸体物理，Character 将通知转发给 GameMode。
5. GameMode 只接受决定本局结果的角色通知，等待 DeathPresentationHoldSeconds 与角色 Feedback 收尾总时长的较大值后广播 MatchPresentationReady；HUD 此时显示结算并开放鼠标与 Retry。

## 重试

Retry 经 PlayerController 转给 GameMode，只接受本地第一个 PlayerController，且要求结算已就绪、本局未请求过；请求成功后按钮禁用。

重试不重置任何状态，直接重载当前关卡，入场、GAS 初始化和 Boss AI 全部重走；重载失败回滚请求标志。
