# Contributing

Nodo contributions should preserve protocol safety and auditability.

## Workflow

1. Create a focused branch.
2. Keep changes small and reviewable.
3. Build and test before submitting code changes.
4. Document behavior that affects operators, storage, governance, treasury, economics, or security.
5. Avoid mixing unrelated refactors with protocol changes.

## Rules

- Do not disable tests to pass a change.
- Do not hide validation failures.
- Do not weaken security checks.
- Do not create fake transports, fake governance, fake treasury approvals, or fake tests.
- Keep comments in English and only where they explain non-obvious reasoning.
- Prefer deterministic tests and canonical serialization.

Root contribution policy: [CONTRIBUTING.md](../../CONTRIBUTING.md).
