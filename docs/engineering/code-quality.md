# C++ Code Quality Guidelines

These guidelines apply to C++ implementation, code review, and AI-assisted changes under `Source/`.

Follow these sources for formatting and basic conventions, in order of precedence:

1. Epic Unreal Engine C++ Coding Standard.
2. The repository root `.editorconfig`.
3. Existing project interfaces, architecture, and lifecycle conventions.

Before making changes, read the relevant implementation, interface contracts, and project documentation. Confirm call order, synchronous callbacks, ownership, and cleanup behavior. Preserve unrelated uncommitted changes and modify only code within the task's scope.

## Required Boundaries

- Keep module dependencies one-way: `AshenOath` composes `AshenOathCombat`; the reusable module must not depend on concrete Player, Boss, or UI types.
- Make ownership explicit when creating, activating, or cleaning up execution state. Record the necessary local ownership before calling an API that may synchronously invoke Gameplay code or callbacks. After the call returns, verify that the current execution is still valid.
- Cleanup functions must revoke only the state they own, tolerate repeated calls, and avoid clearing state owned by another execution.
- Preserve input checks, ownership checks, and failure handling that address real cases. Do not add defensive branches without a concrete failure scenario.

## Preferred Practices

- For complex lifecycles, let the entry point show the normal business sequence and failure exits. Move distinct stages and UE/GAS details into clearly named functions.
- A distinct business purpose, an ownership change, or a call that may synchronously invoke external callbacks is a reason to consider splitting a function. Add a function only when the split reduces the cost of understanding the code; do not split mechanically by line count or a fixed number of layers.
- Keep validation functions focused on checking conditions. Do not hide state changes, GameplayEffect application, event broadcasts, or Task activation in them. Keep post-activation ownership and validity checks near the corresponding stage so that the order of events stays visible.
- When several parameters form a meaningful business request, consider a `struct`. Do not introduce a wrapper solely to reduce the parameter count. Avoid anonymous `bool` parameters for business modes; use a named enum when the modes have distinct meanings.
- Choose UObject reference types based on lifetime: a raw pointer may be used for a synchronous borrow; consider `TWeakObjectPtr` for a non-owning reference retained across frames; use an appropriate `UPROPERTY` reference for a persistent member that needs GC tracking. Use `USTRUCT` only when engine features such as reflection are needed.
- Prefer clear names, `const`, and explicit control flow. Comments should explain non-obvious contracts, reasons, units, ownership, or engine constraints rather than restating each line of code.

Orchestration, validation, creation or reservation, activation, cleanup, and notification or presentation are useful categories for spotting mixed responsibilities. They are not a required classification for every function. Do not add classes, state machines, or abstraction layers merely to fit these categories.
