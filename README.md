# Nodo

Nodo is an experimental C++ foundation for the Nodo Protocol, a
Proof-of-Protection blockchain protocol focused on network protection,
auditable state transitions, safe finality and damage reduction when nodes or
validators misbehave.

Nodo is not a production mainnet. The current `localnet` path is intentionally
small, but it uses the same protocol pipeline that future `testnet` and
`mainnet` configurations should use: transaction admission, candidate block
production, state-transition validation, validator votes, quorum certificate,
finalization, persistence, reload and chain audit.

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

## Localnet Flow

```bash
build/nodo init --data-dir .nodo
build/nodo keys create --data-dir .nodo
build/nodo tx submit --data-dir .nodo
build/nodo block produce --data-dir .nodo
build/nodo node reload --data-dir .nodo
build/nodo chain audit --data-dir .nodo
build/nodo status --data-dir .nodo
build/nodo inspect --data-dir .nodo
```

Compatibility commands such as `submit-demo-transaction`, `produce-demo-block`
and `reload` still exist while operator muscle memory migrates. They print
deprecation warnings and run the same localnet protocol path.

Current limitations are explicit:

- `localnet` uses a temporary deterministic local signature provider;
- `localnet` keys are stored by `KeyStore` in an unencrypted local format that
  is not production-safe yet;
- `localnet` uses explicit development account allocations in `GenesisConfig`
  for bootstrap validators so balance and nonce checks can run locally;
- no P2P validator networking is included in this phase;
- no slashing or production mainnet path is included in this phase;
- mempool future nonces are rejected until a full per-account queue exists;
- coin-lot ownership, double-spend and complete supply audit must continue
  moving behind the state-transition validation gate.

The runtime manifest stores `latestStateRoot`, and reload rebuilds state from
genesis through finalized blocks before accepting the manifest tip. Chain audit
reports the same root together with finalized height and hash.

## Documentation

- [Architecture](docs/ARCHITECTURE.md)
- [Protocol](docs/PROTOCOL.md)
- [State transition](docs/STATE_TRANSITION.md)
- [Consensus rules](docs/CONSENSUS_RULES.md)
- [Security model](docs/SECURITY_MODEL.md)
- [Networks](docs/NETWORKS.md)
- [CLI](docs/CLI.md)
- [Node data directory](docs/NODE_DATA_DIRECTORY.md)
- [Development mode](docs/DEVELOPMENT_MODE.md)
- [Roadmap](docs/ROADMAP.md)
