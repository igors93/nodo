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
- `serialization`: deterministic text codecs used by current development
  persistence and state rebuilds.
- `crypto`: hash, key, signature-provider and development-signature boundaries.
- `economics`: protection work, emissions, rewards, validator scores and
  penalties.
- `privacy`: experimental private accounting records and nullifier tracking.
- `p2p`: local peer messages and sync planning; no socket server yet.

## Persistence Flow

The local node directory stores a manifest, finalized block files, a runtime
snapshot and pending mempool transactions. Runtime reload is rebuild-first:

```text
manifest -> genesis runtime -> finalized blocks -> persistent mempool -> audit
```

The manifest is accepted only when the rebuilt latest block height and hash
match it.

## Build

CMake builds one `nodo_core` static library, one `nodo` CLI executable and one
test executable per `tests/<module>/*.cpp` file. CTest labels tests by module.
