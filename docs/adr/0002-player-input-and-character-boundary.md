# ADR-0002：Controller 管输入，Character 管角色移动规则

状态：已采用。记录日期：2026-09-05。适用范围：当前 Move/Look 与自由镜头。

## 背景

设备输入依赖 LocalPlayer 和输入组件；角色是否允许移动则依赖当前身体及其 Gameplay 状态。占有关系会变化，输入组件就绪和角色占有也不是同一个事件。需要避免绑定叠加、失去控制后残留映射，以及输入层逐渐承担角色规则。

## 决定

PlayerController 管理 Move/Look 绑定和专用 Mapping Context，将移动意图及参考视角交给当前 Character。Character 负责角色侧约束与方向转换，CharacterMovement 执行移动。

自由视角由 Controller 更新 ControlRotation，镜头组件由 Character 持有。输入映射的归属、清理和接口约定见 [玩家控制系统说明](../systems/player-control-camera.md)。

## 方案与取舍

| 方案 | 收益 | 当前取舍 |
|---|---|---|
| Character 同时绑定输入、管理映射并执行移动 | 单一 Pawn 示例中链路较短 | 会将本地输入资源与角色行为耦合；本项目选择拆开 |
| Controller 直接承担全部角色规则 | 输入到行为集中 | 死亡、动作限制等规则容易进入 Controller，其他行为入口难以复用 |
| Controller 路由，Character 接收语义请求（采用） | 输入生命周期与角色行为各有负责人 | 需维护请求接口；Controller 当前仍显式依赖玩家 Character 类型 |
