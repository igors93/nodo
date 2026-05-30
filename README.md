# Nodo

Nodo is an experimental C++ blockchain node focused on network protection,
auditable state, reliable local persistence and useful validator work.

The project is not production-ready. Current cryptography uses a real SHA-256
hash boundary, but signatures and block production still run in a development
mode so the runtime can be exercised end to end.

## Current Shape

- `apps/cli/`: CLI entrypoint.
- `include/` and `src/`: modules for app, core, consensus, crypto, economics,
  mempool, node runtime, privacy, serialization, storage and P2P foundations.
- `tests/<module>/`: CTest-discovered module tests.
- `docs/`: stable project docs plus detailed module guides.

## Quickstart

```bash
./scripts/cmake_build.sh
./scripts/cmake_test_all.sh
```

The CLI binary is written to:

```text
build/nodo
```

## Local Development Flow

```bash
build/nodo init --data-dir .nodo
build/nodo submit-demo-transaction --data-dir .nodo
build/nodo produce-demo-block --data-dir .nodo
build/nodo reload --data-dir .nodo
build/nodo status --data-dir .nodo
build/nodo inspect --data-dir .nodo
```

Demo commands are intentionally local development commands. They use fake
development signatures and are isolated from any claim of production consensus.

## Documentation

- [Architecture](docs/ARCHITECTURE.md)
- [CLI](docs/CLI.md)
- [Node data directory](docs/NODE_DATA_DIRECTORY.md)
- [Development mode](docs/DEVELOPMENT_MODE.md)
- [Roadmap](docs/ROADMAP.md)
