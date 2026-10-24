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

MatchOutcomeChanged 即时报告胜负，不直接触发结算 UI。HUD 初始化时也查询展示是否已就绪，避免晚订阅漏掉事件。

Retry 经 PlayerController 请求 GameMode，结算展示就绪后重新加载当前关卡；角色和局内状态随关卡重建。

## 反馈

Feedback 监听 Damage / Death：Applied 在 Bounds 中心播放独立接触音，包括致命一击；非致命 Applied 另请求疼痛人声并播放命中粒子，不要求受击资格。疼痛人声有独立冷却。进入死亡后由 DeathStarted 停止旧人声并播放一次死亡反馈，绕过普通冷却。

攻击与闪避的 Combat Vocal Notify 只提交机会：声音素材由 Notify 指定，概率、音量、音高变化与跨动作冷却由角色 Feedback 的 VocalRules 配置。Ability 确认 Montage 已播放后登记动作，在 Notify 入口核对 Mesh、Montage 与播放实例 ID。普通人声在整套动作内共用一次额度；玩家四连击前三下随机，最后一下使用独立的 HeavyComboFinisher 额度，配置为 100% 概率并绕过普通冷却，每套最多一次。Boss 的 ComboFinisher 仍使用普通额度。人声标记应放在 Montage 开始之后，零时刻标记不会在动作登记前补播。

只有成功发起播放才消耗动作额度与冷却；未通过的请求直接丢弃。优先级为死亡 > 疼痛 > 特殊招式 > 重击/重击终结 > 普通攻击/普通终结/闪避，同级不打断。重击终结可替换尚未结束的轻击人声，受伤和死亡仍优先；没有打到最后一下时不会补播。特殊招式绕过普通冷却，保留作者配置的提示时机。SoundCue 选择素材变体，角色人声使用独立、按 Owner 限制为一声的 Concurrency，RetriggerTime 为零。

Ability 正常完成后允许当前短人声自然结束，取消时停止属于该动作的人声；旧动作的清理不能关闭新动作。StopCombatForMatchEnd 禁止后续普通人声并停止存活方人声，死亡人声不受 Character 与 Feedback 回调顺序影响。EndPlay 停止剩余人声。武器挥动、接触音和音乐不经过人声调度。

Feedback 不拥有死亡 Montage 或结算计时；通用 Death 不引用角色、GameMode 或 UI。伤害与受击规则见 [战斗动作](combat-actions.md)。

死亡动画结束后，项目 Feedback 关闭 Capsule 碰撞，以角色 Mesh 配置的 PhysicsAsset 接入 Ragdoll。落地反馈通过地面 Trace 与模拟碰撞形状的最近点距离判断：距地面 6 cm 以内时，在地面接触处播放一次落地音和灰尘；不依赖资产的 Hit Event 配置，擦墙不消耗落地反馈。玩家保留尸体并降低配置的发光参数；Boss 使用保留原皮肤纹理的死亡材质副本，以 DeathProgress 驱动世界坐标噪声侵蚀及绿色边缘发光，收尾后隐藏身体。魂火与余烬使用项目内专用粒子及材质。镜头在死亡与收尾期间平滑追踪身体并向肩侧偏移，避免玩家遮住 Boss。Feedback 收尾后令尸体休眠并停止 Tick，退出时停止剩余粒子。无 PhysicsAsset 时跳过物理接管，不阻塞结算。

## 踩坑/变更

- 死亡 Montage 缺失或播放失败，也要在 DeathStarted 广播后报告表现结束，不能卡住结算。
- 演出中结算或退出同样释放 Sequence / SoundMix；EndPlay 清除结算计时器与音乐。
