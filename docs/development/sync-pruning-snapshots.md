# Sync, Pruning and Snapshots

Nodo's target architecture separates two duties: **verifying the current
state** with bounded storage and **preserving the complete history** through
archive nodes and Proof of Archival. The normative rules are
in [ADR 0014](../spec/adr-0014-bounded-storage-and-proof-of-archival.md);
this page explains how the code implements them and what is not active yet.

## Node storage modes

| Mode | Keeps | Notes |
| --- | --- | --- |
| `ARCHIVE` (default) | Everything | Full genesis-to-tip audit; may run Proof of Archival. |
| `NORMAL` | State, checkpoints, headers, QCs, recent blocks | Old checkpoint snapshots and the legacy snapshot cache are pruned. Block bodies stay until checkpoint-base reload lands (roadmap 3.17). |
| `LIGHT` | Headers, QCs, checkpoints, proofs | Same storage rules as `NORMAL` with the minimum window; light verification itself is `LightClientService`. |

## Finalized state checkpoints

`node::FinalizedStateCheckpoint` (in `node/history`) is created automatically
by `CheckpointService::onBlockFinalized` after `FinalizedBlockStore` has made
a block durable, at every multiple of `checkpointIntervalBlocks`. It reuses
the block's own PRECOMMIT QC: the QC signs the block hash, which commits the
header state root. A checkpoint failure is logged and never undoes the final
block. Each checkpoint links to the previous one; an existing chain gets its
missing checkpoints with `nodo checkpoint backfill`, which replays every
stored block and re-derives identical checkpoint ids.

```bash
nodo checkpoint status|list [--json]
nodo checkpoint show [--height H]
nodo checkpoint verify [--height H]   # id, snapshot, QC, link, archive index
nodo checkpoint backfill
```

## Full protocol state snapshots

`node::FullProtocolStateSnapshot` wraps the existing `FastSyncSnapshot`
(accounts and every domain payload) and the validator-set window that
still-admissible evidence can reference. `FullProtocolStateSnapshotVerifier`
recomputes the protocol state root from the carried data, decodes every
domain strictly and checks the window against the QC and the deterministic
next-set projection. `FastSyncSnapshot::verifiesProtocolStateRoot` now runs
on the legacy fast-sync path too.

## Bootstrap from a checkpoint

`CheckpointBootstrapVerifier` turns untrusted peer data into a verified tip:
trusted anchor, checkpoint id, identity, snapshot digest, state root,
validator window, QC, then every later block with its own QC and replayed
state root. The anchor (`<height>:<checkpointId>`) must come from out of
band and be no older than the 14-day weak-subjectivity window.

```bash
nodo checkpoint verify-bootstrap --source-dir PEER_DIR \
  --trusted-checkpoint 43200:<checkpoint-id>
```

`SnapshotChunkServer` and `SnapshotAssembler` move snapshots in fixed
chunks verified one by one against the checkpoint digest.

**Not active yet:** hydrating a live runtime from a verified checkpoint and
persisting it. The runtime reloads by replaying from genesis, so a node
started from a snapshot could not restart (roadmap 3.17). The existing
in-memory `PersistentBlockStateSyncApplier::importSnapshot` path now rejects
snapshots whose data does not hash to the state root, but it is still not
wired to the network (roadmap 5.3).

## Retention and pruning

`HistoryRetentionPolicy` computes a floor per data category (table in ADR
0014); block bodies may only go below the minimum over the retention window,
the verified checkpoint base, the pending reward-settlement epoch, open
governance, treasury timelocks and proven archival replication.
`HistoryPruningEngine` applies it with a crash-safe journal:

```text
PREPARE  journal.nodo (plan id, targets, manifest to commit)
DELETE   allow-listed regular files only, idempotent
COMMIT   manifest.nodo atomically, then remove the journal
```

`RuntimeStateLoader` recovers an interrupted run before it trusts the pruning
manifest; a tampered journal is quarantined, startup fails, and nothing is
deleted.

```bash
nodo pruning status [--mode normal|light|archive]
nodo pruning run --mode normal [--retain-blocks N] [--retain-snapshots N] [--dry-run]
```

Today a `NORMAL` run removes old checkpoint snapshots and old legacy
fast-sync snapshots; finalized block files are always kept, with the
blocker reported by `pruning status`. The legacy `LIGHT` policy deleted
block files that reload still needs; it is no longer applied. The legacy
fast-sync snapshot store, which wrote a full account snapshot on every
block, now keeps only the newest snapshot.

## Audit modes

| Mode | Scope | Who |
| --- | --- | --- |
| Full historical audit | Genesis to tip (`chain audit`, `checkpoint backfill`) | Archive nodes |
| Checkpoint-forward audit | Verified checkpoint to tip (`checkpoint verify-bootstrap`) | Normal nodes |

A checkpoint-forward audit proves that the current state follows from the
checkpoint; it does not re-prove history before it.

## Archive segments and Proof of Archival

Sealed segments are committed at every checkpoint
(`history/archive/commitments`). `nodo archive self-audit` challenges the
node's own copy with the exact protocol: derive a challenge from finalized
data, build a proof from the local block files, verify it. Network
challenges, on-chain proofs and rewards wait for a protocol upgrade
(roadmap 6.9).

```bash
nodo archive status|segments|enable|self-audit [--segment N]
```

## Storage schema

Storage schema v2 adds the `history/` tree. Version 1 directories still load;
`nodo storage migrate` upgrades them explicitly and idempotently.
