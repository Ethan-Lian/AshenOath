# ADR-0002：Controller 管输入，Character 管角色规则

已采用 · 2026-09-05

## 背景

本地输入依赖 LocalPlayer，移动和动作约束依赖当前身体。输入组件就绪与占有并非同一个事件。

## 决定

Controller 管绑定和 Mapping Context，向当前 Character 转发移动意图、参考 Yaw 和动作请求。Character 处理状态约束、方向转换、锁定和 Ability 请求，CharacterMovement 执行移动。

## 取舍

输入资源与角色行为分开，其他行为入口也能复用角色规则。代价是维护请求接口，Controller 仍显式依赖玩家 Character 类型。

映射生命周期、镜头和朝向行为见 [玩家控制与镜头](../systems/player-control-camera.md)。
