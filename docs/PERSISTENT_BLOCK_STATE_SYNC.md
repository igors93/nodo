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
4. request the next bounded block window when the remote peer is ahead — all height
   gaps, regardless of size, are handled with incremental `REQUEST_BLOCKS`;
5. validate that a block batch is consecutive and connects to the local checkpoint;
6. advance the durable checkpoint only after the batch shape is accepted;
7. encode/decode checkpoints, block sync batches and snapshot manifests using canonical bytes.

## Security rules

- Sync progress is explicit and durable.
- A batch must start at `checkpoint.finalizedHeight + 1`.
- The first batch block must reference the checkpoint block hash.
- Later blocks in the batch must link to the previous item hash.
- Oversized batches are rejected.
- Snapshot manifests are represented separately from block batches.
- Canonical codecs require full payload consumption.

## Snapshot sync

`importSnapshot` explicitly returns `REJECTED` with a clear diagnostic. Snapshot
sync requires full runtime hydration — account state, validators, coin lot
registry, staking positions, governance store — and is not yet implemented. The
planner (`planFromRemoteStatus`) no longer routes any gap to `REQUEST_SNAPSHOT`;
all gaps use incremental block sync. Snapshot sync will be introduced in Phase 6
once state snapshot creation and hash verification are complete.

## Not included yet

- Merkle proof based snapshot verification.
- Snapshot sync (runtime hydration from a snapshot manifest).

The sync planner, codec and applier are wired into the live node path through
`NodeOrchestrator` (`planFromRemoteStatus`, `encodeBlockSyncBatch`,
`importFinalizedBatch`), so finalized batches flow through the canonical
runtime pipeline during normal operation.

## Slashing evidence and penalty audit

Persistent block sync treats finalized slashing evidence as block data, not as optional gossip state. During finalized-batch import, each block is committed through the canonical runtime pipeline and then audited: every finalized evidence id must have a matching penalty decision, validator jail/tombstone status and bounded staking slash effect. Pending local evidence with the same id is pruned from the evidence pool/store after the batch checkpoint is safely persisted.
