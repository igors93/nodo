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
```

## Storage schema

Before the manifest is trusted, the loader validates the storage schema. Unknown schema ids, missing schema files, future versions, unsafe downgrades, and malformed files must be rejected.

Nodo should not perform implicit storage migration. Migration must be explicit, versioned, and test-covered.

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
