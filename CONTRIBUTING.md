# Contributing to Nodo

Nodo is security-first infrastructure. Contributions should make the protocol easier to verify, audit, operate, or safely extend.

## Before You Start

- Read [Project Overview](docs/overview/project-overview.md).
- Read [Proof of Protection](docs/overview/proof-of-protection.md).
- Read [Development Guide](docs/development/contributing.md).

## Development Workflow

1. Work on a focused branch.
2. Keep changes scoped to one concern.
3. Update documentation when behavior changes.
4. Build and test code changes before submitting.
5. Do not include unrelated formatting churn.

## Build and Test

Windows:

```powershell
.\scripts\cmake_build.bat
.\scripts\cmake_test_all.bat
```

Unix-like:

```bash
./scripts/cmake_build.sh
./scripts/cmake_test_all.sh
```

Documentation-only changes do not require a full build unless they edit build scripts or generated behavior.

## Contribution Rules

- Do not disable or weaken tests to make a change pass.
- Do not remove protocol validation, P2P validation, storage validation, treasury validation, governance validation, or security gates.
- Do not create fake transports, fake approvals, fake governance, or fake evidence in production code.
- Keep comments in English and use them only when they clarify non-obvious behavior.
- Keep mainnet blocked unless production readiness has been explicitly achieved and reviewed.

## Review Expectations

Pull requests should explain:

- what changed;
- why it is safe;
- which invariants are protected;
- which tests or documentation were updated;
- any known limits or follow-up work.
