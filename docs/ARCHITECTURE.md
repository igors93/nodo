# Nodo Architecture

Nodo is organized by runtime responsibility rather than implementation phase.

## Modules

- `app`: command-line interface and local development commands.
- `core`: blocks, blockchain, transactions, ledger records, state, validators
  and coin lot rules.
- `consensus`: votes, quorum certificates, finalization and fork choice.
- `mempool`: volatile transaction admission and selection.
- `node`: local runtime, data directory, runtime loader, finalized block store,
  persistent mempool and local block pipeline.
- `storage`: generic chain/block storage helpers and atomic file IO.
- `serialization`: strict canonical binary codecs for protocol boundaries plus
  deterministic text codecs for legacy development persistence.
- `crypto`: hash, key, signature-provider and development-signature boundaries.
- `economics`: protection work, emissions, rewards, validator scores and
  penalties.
- `privacy`: experimental private accounting records and nullifier tracking.
- `p2p`: peer messages, sync planning, gossip mesh, loopback/TCP transports and
  encrypted peer-channel framing for testnet development.

## Persistence Flow

The local node directory stores a manifest, finalized block files, a runtime
snapshot and pending mempool transactions. Runtime reload is rebuild-first:

```text
manifest -> genesis runtime -> finalized blocks -> persistent mempool -> audit
```

The manifest is accepted only when the rebuilt latest block height, hash and
`latestStateRoot` match it. Finalized block files carry a `postStateRoot`,
quorum certificate and finalized record; reload verifies all three before the
runtime is considered auditable.

## Build

CMake builds one `nodo_core` static library, one `nodo` CLI executable and one
test executable per `tests/<module>/*.cpp` file. CTest labels tests by module.
