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

- `localnet` uses OpenSSL Ed25519 for user transactions and blst BLS12-381 for
  validator operations;
- `localnet` keys are stored by `KeyStore` in an unencrypted deterministic
  local format that is not production-safe custody yet;
- `localnet` uses explicit development account allocations in `GenesisConfig`
  for the default user key so balance and nonce checks can run locally;
- P2P, TCP transport, gossip and encrypted peer-channel foundations exist for
  testnet development, but they are not a production networking stack yet;
- slashing evidence and validator penalty decisions are auditable and
  idempotent, but production stake-slashing and mainnet activation are still
  intentionally out of scope;
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
