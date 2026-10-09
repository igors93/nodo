# Storage and Reload

Storage is part of the protocol safety model. Nodo should not trust a local database merely because files exist.

## Data directory

The default local data directory is `.nodo`.

Typical layout:

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
  sync/
    qc/<height>.qc
  history/                                  (storage schema v2)
    checkpoints/<height>.checkpoint
    checkpoints/conflicts/<height>-<id>.checkpoint
    snapshots/<height>.snapshot
    archive/commitments/<segment>.segment
    archive/self_audit.nodo
    pruning/manifest.nodo
    pruning/journal.nodo
```

The `history/` tree holds finalized state checkpoints, their full protocol
state snapshots, archive segment commitments and the crash-safe pruning
manifest and journal ([ADR 0014](../spec/adr-0014-bounded-storage-and-proof-of-archival.md)).
File names derive only from heights and segment indices; reads are size
capped and refuse symbolic links; an existing checkpoint is never
overwritten by a different one.

`genesis.nodo` is a `NODO_GENESIS_DOCUMENT_V1` genesis document (`config::GenesisDocumentCodec`) written by `init`. For networks without a built-in genesis (`testnet-candidate`), later commands load the genesis from this file, and the manifest's genesis id must match it.

## Storage schema

Before the manifest is trusted, the loader validates the storage schema. Unknown schema ids, missing schema files, future versions, unsafe downgrades, and malformed files must be rejected.

Nodo should not perform implicit storage migration. Migration must be explicit, versioned, and test-covered.

The current node data directory schema is **version 2**. Version 1
directories still load, but the node writes no checkpoint, snapshot or
pruning state into them until the operator runs `nodo storage migrate`. The
migration creates the `history/` tree, converts a legacy pruning manifest
and rewrites the schema file last, so a crash at any point leaves a
directory that still reads as version 1 and the migration can run again.
Binaries that only know version 1 refuse a version 2 directory.

## Manifest

`manifest.nodo` records chain identity and latest finalized state. It must be strict and canonical.

Important fields include:

- chain identity;
- genesis id;
- latest finalized height;
- latest finalized hash;
- latest state root;
- validator count;
- peer count;
- timestamps.

## Finalized block persistence

Finalized blocks are written before the manifest is advanced. Reload must reject:

- missing block files;
- malformed block files;
- non-canonical serialization;
- header/payload mismatch;
- quorum/finalized-record mismatch;
- invalid append order;
- post-state-root mismatch;
- protocol-domain replay mismatch.

## Mempool persistence

Persistent mempool entries are stored separately from finalized history. Reload verifies:

- transaction signature;
- duplicate transaction id;
- duplicate sender/nonce;
- minimum fee;
- nonce against rebuilt account state.

Malformed mempool files should reject reload instead of being silently ignored.

## Atomic writes

Critical files should be written through temporary file plus rename. This protects against partial writes during crashes.

## Reload principle

```text
canonical genesis + finalized blocks + deterministic replay = accepted runtime state
```

If replay does not match persisted commitments, the node must fail safe.

Reload still replays every finalized block from genesis. Checkpoints and
snapshots are verified, durable artifacts, but they are not yet a reload
base, so finalized block files are never pruned (roadmap 3.17). Before it
trusts the pruning manifest, the loader rolls forward an interrupted pruning
run or quarantines a tampered journal.
