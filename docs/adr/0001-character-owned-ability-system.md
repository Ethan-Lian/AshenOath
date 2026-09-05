# ADR-0001：由 Character 持有 Ability System

状态：已采用。记录日期：2026-09-05。适用范围：当前单机玩家与 Boss。

## 背景

玩家与 Boss 都需要属性和 GameplayEffect，但当前没有换 Pawn 后保留能力、跨地图保留战斗属性或多人同步的需求。需要明确 ASC（AbilitySystemComponent）和 AttributeSet 的宿主，以及它们何时可用、何时失效。

## 决定

玩家和 Boss 均由 Character 持有 ASC 与同一种 AttributeSet，GAS 的 OwnerActor 和 AvatarActor 均指向自身，ASC 关闭复制。属性状态随角色存续，不跨角色实例自动保留。

Character 负责 GAS 的初始化和清理；具体时序见 [GAS 系统说明](../systems/ability-system.md)。

## 方案与取舍

| 方案 | 适用收益 | 当前代价 |
|---|---|---|
| Character 持有 ASC，Owner = Avatar（采用） | 宿主与受控身体一致；玩家/Boss 的访问方式一致；生命周期集中 | 销毁角色就失去该实例状态，未来持久化需另作设计 |
| PlayerState 等独立宿主持有 ASC，Character 作为 Avatar | 适合状态跨 Pawn 存续 | 当前需额外协调宿主、Avatar 切换与清理，收益不足；Boss 也未必适合相同宿主 |
