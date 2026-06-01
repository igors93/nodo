# Persistent Block/State Sync

This phase adds the first durable sync boundary for Nodo testnet nodes.

It does not bypass existing block, state or consensus validation. Instead, it adds
clean primitives that let a node decide what it needs from a peer, validate the
shape of received sync batches, and persist sync progress so a restart does not
lose the last verified checkpoint.

## Added modules

- `node::PersistentSyncCheckpoint`
- `node::PersistentSyncCheckpointStore`
- `node::PersistentBlockSyncItem`
- `node::PersistentBlockSyncBatch`
- `node::PersistentSnapshotSyncManifest`
- `node::PersistentBlockStateSyncPlanner`
- `node::PersistentBlockStateSyncApplier`
- `node::PersistentBlockStateSyncCodec`

## What works now

A testnet node can:

1. persist its latest verified sync checkpoint;
2. reload that checkpoint after restart;
3. compare local checkpoint with remote `ChainStatusMessage`;
4. request the next bounded block window when the remote peer is ahead;
5. request a snapshot manifest when the remote peer is far ahead;
6. validate that a block batch is consecutive and connects to the local checkpoint;
7. advance the durable checkpoint only after the batch shape is accepted;
8. encode/decode checkpoints, block sync batches and snapshot manifests using canonical bytes.

## Security rules

- Sync progress is explicit and durable.
- A batch must start at `checkpoint.finalizedHeight + 1`.
- The first batch block must reference the checkpoint block hash.
- Later blocks in the batch must link to the previous item hash.
- Oversized batches are rejected.
- Snapshot manifests are represented separately from block batches.
- Canonical codecs require full payload consumption.

## Not included yet

- Direct mutation of `Blockchain` from sync batches.
- Full block deserialization from network payloads.
- Quorum certificate validation inside the applier.
- Historical replay from disk.
- Merkle proof based snapshot verification.
- Automatic runtime integration into `TcpTestnetNodeRuntime::tick()`.

Those should be implemented after this boundary is stable and after the real
block storage format is made fully canonical.
