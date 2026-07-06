# Fast Sync Snapshot Foundation

This patch introduces Nodo's canonical fast-sync snapshot payload and store.

## What is implemented

- `FastSyncSnapshot`: deterministic snapshot payload with genesis identity, chain id,
  finalized height/hash, protocol state root, account root, protocol-domain digest,
  canonical account-state boundary and snapshot digest.
- `FastSyncSnapshotStore`: atomic snapshot persistence under
  `{dataDir}/runtime/fast_sync_snapshots` with a `latest` pointer.
- `FastSyncSnapshotVerifier`: validates a snapshot against local genesis and a
  `PersistentSnapshotSyncManifest`.
- `FastSyncSnapshotService`: builds snapshots from a live `NodeRuntime`, persists
  them, and verifies/checkpoints matching manifests without unsafe partial
  runtime mutation.
- `NodeDataDirectory::writeRuntimeSnapshot`: now also writes a fast-sync snapshot
  for finalized heights greater than zero.
- `RuntimeBlockPipeline`: epoch snapshot digest now uses the same canonical
  `FastSyncSnapshot` digest that the fast-sync store writes.

## Safety boundary

`PersistentBlockStateSyncApplier::importSnapshot` now verifies local snapshot
payloads against manifests, but it refuses to advance a checkpoint ahead of an
in-memory runtime that has not been hydrated to the same height/hash. This is
intentional: advancing the checkpoint without hydrating governance, staking,
validator, penalty, supply and account domains would corrupt future sync.

The remaining large step is domain payload hydration: each protocol domain must
have a canonical snapshot payload and a strict deserializer before a node can
start from a remote snapshot at height N and continue applying blocks N+1.
