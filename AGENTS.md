# AshenOath Project Guidance
 
## Starting Work

Before implementation:

- Read [`docs/arch/architecture.md`](docs/arch/architecture.md) to understand module boundaries.
- Review relevant files under `docs/adr/` for accepted architectural decisions.
- Read relevant files under `docs/systems/` when the task touches a system's contracts, ownership, lifecycle, or data flow.

## Architecture and Validation

- Preserve the one-way dependency: `AshenOath` composes `AshenOathCombat`; the reusable Combat module must not depend on the AshenOath module.
- For C++ changes and reviews, follow [code quality](docs/engineering/code-quality.md) and `.editorconfig`. Verify Unreal Engine APIs against the actual `.uproject` engine version and available engine source or official documentation.
- Keep generated binary assets, temporary files, and local planning notes outside Git tracking.