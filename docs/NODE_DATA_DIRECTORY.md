# Nodo Data Directory

The default local data directory is `.nodo`.

```text
.nodo/
  storage_schema.nodo
  manifest.nodo
  genesis.nodo
  blocks/
    block_<height>.nodo
  mempool/
    tx_<transaction-id>.nodo
  peers/
    local_peer.nodo
  runtime/
    runtime_snapshot.nodo
```

## Manifest

Before `manifest.nodo` is trusted, the loader validates
`storage_schema.nodo`. The accepted node data directory schema is
`NODO_NODE_DATA_DIRECTORY` version `1`; missing schema files, unknown schema
ids, future versions and unsafe downgrades are rejected. Nodo does not perform
implicit storage migration.

`manifest.nodo` is a strict versioned key-value file. It records chain identity,
genesis id, latest finalized height/hash, `latestStateRoot`, validator count,
peer count and timestamps.

Unknown fields, duplicate fields, malformed lines and unsupported versions are
rejected. The file must remain canonical: reordering fields, removing required
fields or changing the deterministic serialization causes reload to fail.

At genesis, `latestStateRoot` is the deterministic account-state root derived
from `GenesisAccountConfig`. After a finalized block is persisted, the manifest
is advanced only after the block artifact is written, and `latestStateRoot`
must equal that block's `postStateRoot`.

## Finalized Blocks

Finalized blocks are written before the manifest is advanced. A reload rejects
missing block files, malformed files, header/payload mismatch, non-canonical
files, quorum/finalized-record mismatch and blocks that cannot append to the
rebuilt chain.

Reload rebuilds account state from genesis through each finalized block. Every
stored `postStateRoot` must match the preview result for that height, and the
rebuilt tip root must match `manifest.latestStateRoot`.

## Persistent Mempool

Pending transactions are stored as one versioned key-value file per transaction.
Malformed `.nodo` mempool files reject runtime reload instead of being silently
ignored. Files ending in `.tmp` are not canonical mempool entries. Finalized
transactions are removed from the persistent mempool.

Persistent mempool reload verifies the transaction signature, duplicate
transaction id, duplicate sender/nonce, minimum fee and nonce against the
rebuilt account state. Until a full per-account transaction queue exists, a
future nonce is rejected instead of being parked silently.

## File Writes

Manifest, runtime snapshots, finalized blocks and persistent mempool entries use
temporary-file plus rename writes through `storage::AtomicFile`.
