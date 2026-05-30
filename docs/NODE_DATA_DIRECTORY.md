# Nodo Data Directory

The default local data directory is `.nodo`.

```text
.nodo/
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

`manifest.nodo` is a strict versioned key-value file. It records chain identity,
genesis id, latest finalized height/hash, validator count, peer count and
timestamps.

Unknown fields, duplicate fields, malformed lines and unsupported versions are
rejected.

## Finalized Blocks

Finalized blocks are written before the manifest is advanced. A reload rejects
missing block files, malformed files, header/payload mismatch and blocks that
cannot append to the rebuilt chain.

## Persistent Mempool

Pending transactions are stored as one versioned key-value file per transaction.
Malformed `.nodo` mempool files reject runtime reload instead of being silently
ignored. Finalized transactions are removed from the persistent mempool.

## File Writes

Manifest, runtime snapshots, finalized blocks and persistent mempool entries use
temporary-file plus rename writes through `storage::AtomicFile`.
