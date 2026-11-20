# ADR-0001：Character 持有 Ability System

已采用 · 2026-09-05

## 背景

当前是单机玩家与 Boss，没有跨 Pawn 保留能力、跨地图保留属性或网络复制的需求。

## 决定

Character 持有 ASC 和 AttributeSet，OwnerActor = AvatarActor = 自身，ASC 关闭复制。

## 取舍

宿主和身体的生命周期一致，玩家与 Boss 使用同一套访问方式。角色销毁后状态随之丢失；以后需要跨 Pawn 存续时再考虑 PlayerState 等独立宿主。

初始化和退出见 [GAS 与属性](../systems/ability-system.md)。
