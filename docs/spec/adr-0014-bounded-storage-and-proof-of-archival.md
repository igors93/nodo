# ADR 0014: Bounded node storage, finalized state checkpoints and Proof of Archival

**Status:** accepted design decision. Checkpoints, full protocol state
snapshots, checkpoint-anchored bootstrap verification, the retention policy,
the crash-safe pruning engine, archive segment commitments and the Proof of
Archival reference (challenges, proofs, scoring, replication and rewards)
are implemented as reference components in `nodo/0.7`. Three parts are **not active yet** and are
production gates tracked in the [roadmap](../roadmap.md): deleting finalized
block bodies (item 3.17), on-chain Proof-of-Archival transactions and reward
minting (item 6.9), and live peer-to-peer distribution of checkpoints,
snapshots and archival messages (item 5.9).

## Problem

A blockchain whose every node keeps every block forever makes the storage a
participant needs grow without bound. Nodo separates two duties that are
often confused:

1. **verifying the current state safely**; and
2. **preserving the complete history**.

A participant must be able to do (1) with bounded storage. The network as a
whole must keep doing (2), and the participants who do it must be able to
prove it and be paid for it.

> Users do not need to carry the whole history, but the network rewards the
> participants who preserve it and keep proving that it is still available.
> Historical preservation is protection work. It never replaces consensus.

## Node storage modes

| Mode | Keeps | Duties |
| --- | --- | --- |
| `LIGHT` | Headers, QCs, checkpoints, proofs, minimum window | Verifies finality evidence and inclusion proofs. Does not validate new blocks. |
| `NORMAL` | Current state, checkpoints, every header and QC, a bounded window of full blocks after a verified checkpoint, mempool, recent evidence | Validates and votes. May prune old block bodies once every rule below allows it. |
| `ARCHIVE` | Every finalized block, record, QC, checkpoint and snapshot | Serves full history, runs the genesis-to-tip audit and may take part in Proof of Archival. |

`ARCHIVE` is the default. The legacy pruning name `FULL` maps to `NORMAL`.

## Finalized state checkpoints

A checkpoint is a commitment to the complete protocol state at a height the
existing BFT consensus already finalized. **It adds no vote and no
authority.** The block hash commits the header state root, and the block's
own PRECOMMIT quorum certificate signs that hash, so the existing QC already
certifies the state root:

```text
finalized block -> canonical state -> stateRoot (in the header)
  -> block hash -> PRECOMMIT QC -> FinalizedStateCheckpoint
```

Checkpoints are cut at every positive multiple of `checkpointIntervalBlocks`
(per network profile). Every node that validated the chain builds the same
checkpoint; none can be created from operator input.

The checkpoint body commits, in this canonical order (CanonicalWriter
encoding, schema `NODO_FINALIZED_STATE_CHECKPOINT_V1`, version 1):

```text
networkName, chainId, genesisConfigId, protocolVersion, historyParametersId,
height, epoch, blockHash, previousBlockHash, blockTimestamp,
stateRoot, accountsRoot, validatorSetRoot, nextValidatorSetRoot,
consensusContextDigest, snapshotDigest,
archiveSealedSegmentCount, archiveIndexRoot,
previousCheckpointHeight, previousCheckpointId
```

- `stateRoot` is the existing canonical protocol state root, which already
  commits accounts and every protocol domain (supply, burns, staking,
  validators, validator weights, slashing and governance). No second root
  per domain is invented. `accountsRoot` is its account leaf, kept for light
  client account proofs. Treasury, governance, rewards and penalties are
  committed through these domains; coin lots are not part of the live state
  root yet and enter the snapshot as a domain when they do (roadmap 6.7).
- `validatorSetRoot` is the frozen set that signed this height (it must equal
  the QC's set root); `nextValidatorSetRoot` is the deterministic projection
  for the next height.
- `consensusContextDigest` commits the validator-set history window carried
  by the snapshot (below).
- `snapshotDigest` is the chunked Merkle root of the snapshot encoding.
- `archiveIndexRoot` commits every sealed archive segment commitment.
- `historyParametersId` binds the network-wide history parameters, so nodes
  with different parameters disagree on the id instead of diverging.

`checkpointId = H("HISTORY/CHECKPOINT", body)` excludes the QC: honest nodes
holding different valid vote subsets for the same block derive the same id.
The id is what operators exchange out of band as a weak-subjectivity anchor.

A checkpoint is accepted only if: it is structurally valid and canonically
encoded; its identity, protocol version and parameter id match the local
genesis; its height is on the schedule; it links to the previous scheduled
checkpoint; its archive count covers every sealed segment; its QC names the
canonical quorum, is for exactly this block and parent, and verifies against
the validator set the checkpoint commits. A second checkpoint with a
different id at a stored height is a **safety fault**: it is never
overwritten; it is preserved under `history/checkpoints/conflicts/`.

## Full protocol state snapshot

`FullProtocolStateSnapshot` is a representation of the canonical state at a
checkpoint, never a second source of truth:

- `state`: the existing `FastSyncSnapshot` (accounts and every protocol
  domain payload) with `createdAt` equal to the block timestamp, so every
  honest node encodes identical bytes;
- `consensusWindow`: the frozen validator sets for heights
  `[max(1, H+1-kEvidenceMaxAgeBlocks), H+1]`, so equivocation evidence that
  is still admissible after `H` (ADR 0004) can be verified.

A snapshot is accepted against a checkpoint only if all of these hold:
identity matches; height, block hash and block timestamp match; **the state
root recomputed from the carried accounts and domain payloads equals the
checkpoint state root**; every domain decodes strictly; the window covers
exactly the evidence age, changes only at epoch boundaries, its set at `H`
matches the QC set root, its set at `H+1` equals the deterministic
`ValidatorSetSchedule` projection of the committed validators domain, and
its digest matches; the chunked digest matches; the size is within
`maxSnapshotBytes`.

Recomputing the state root is mandatory everywhere a snapshot enters the
node, including the legacy fast-sync manifest path. Before this ADR, that
path checked only the account root against the accounts the same peer sent,
so a peer could pair a genuine `(blockHash, stateRoot)` with fabricated
balances.

## Bootstrap from a checkpoint

A new node never trusts the peer that serves the data:

```text
trusted anchor "<height>:<checkpointId>" (out of band, ADR 0004)
  -> served checkpoint has exactly that id
  -> anchor is no older than weakSubjectivitySeconds (<= 14 days)
  -> identity and parameters
  -> snapshot digest (before decoding) -> state root, domains, window
  -> QC verified against the committed set
  -> every later block: parent link, header hash, its own QC against the
     frozen set for its height, deterministic replay, header state root
  -> verified tip state
```

Without an anchor the bootstrap is refused: a peer can fabricate a
self-consistent history signed by keys whose stake has long unbonded
(long-range attack), and replay from genesis does not resolve that either.
Historical validator sets in the window are trusted through the anchor; the
current set is additionally proven by the QC. Authenticated set-transition
chains that remove the out-of-band step are a Phase 5 item.

Snapshots travel in fixed chunks. Because `snapshotDigest` is the Merkle
root over those chunks, each chunk is verified against the trusted
checkpoint as it arrives. A served manifest is advisory; memory grows only
with verified bytes; nothing is ever decompressed.

## Retention and pruning

Pruning is never "delete blocks older than X". Each category has its own
rule; block bodies may go only below the **minimum** over all of them:

| Category | Rule |
| --- | --- |
| Genesis, network identity, protocol version, schema | Permanent |
| Checkpoint records with QCs, archive segment commitments | Permanent |
| Finalized headers and QCs | Permanent (light clients and audits) |
| Block bodies | Last `pruningRetentionBlocks` (at least the network minimum) |
| Replay base | Bodies above the verified checkpoint base stay |
| Reward settlement | Every block of the epoch whose settlement is pending |
| Governance in progress | From the creation of the oldest open proposal |
| Treasury timelocks | From the oldest approved or queued action |
| Archive availability | Only segments proven by `archiveMinProvenReplicasBeforePrune` distinct operators |
| Validator-set history, slashing evidence | Evidence admission window (ADR 0004), carried by the snapshot window |
| Checkpoint snapshots | Newest `minimumRetainedCheckpointSnapshots` and the base; archives keep all |
| Legacy fast-sync snapshots | A per-block derived cache: only the newest are kept |

The base is the newest checkpoint past `checkpointConfirmationBlocks` whose
id, snapshot and QC re-verify locally.

**Block bodies are not deleted yet.** The runtime still reloads by replaying
every block from genesis, keeps the chain in memory and indexes blocks by
height, and validator scores are derived by scanning every past settlement
block instead of being committed in the state root. Deleting bodies today
would leave a node unable to restart or diverging on the next reward
settlement. The engine therefore keeps a hard blocker until checkpoint-base
reload lands (roadmap 3.17). The legacy `LIGHT` policy, which deleted block
files and broke restarts, is never re-applied.

Pruning is crash safe:

```text
PREPARE  journal.nodo: plan id, every target, the manifest to commit
DELETE   remove each target (idempotent; regular files only, no symlinks)
COMMIT   write manifest.nodo atomically, then remove the journal
```

Recovery at startup rolls an interrupted run forward. A plan only ever names
data every rule already released, and that never becomes unsafe as the chain
grows. A journal that fails strict parsing, self-hash or the path allow-list
(`history/snapshots/<n>.snapshot`, `runtime/fast_sync_snapshots/<n>.fastsnap`,
`blocks/block_<n>_<hash>.nodo`) is quarantined and nothing is deleted. The
manifest only moves forward after the deletions, so a restart never believes
an incomplete run succeeded.

## Audit modes

- **Full historical audit** (archive nodes): genesis to tip.
  `nodo checkpoint backfill` replays every stored block and re-derives every
  checkpoint; independent nodes obtain identical checkpoint ids.
- **Checkpoint-forward audit** (normal nodes): from a verified checkpoint to
  the tip, with the same QC and state-root checks for every block after it.
  It proves that the current state follows from the checkpoint; it does not
  re-prove history before it. Reward settlement cross-checks for an epoch
  need that epoch's blocks, which the retention rules keep.

## Archive segments

History is cut into segments of `archiveSegmentBlocks`: segment `k` holds
heights `[k*S+1, (k+1)*S]`; the genesis document is retained permanently
instead. A segment is sealed once its last height is final; checkpoints seal
whole segments (`checkpointIntervalBlocks` is a multiple of `S`).

The segment stream is `u64(height) || u32(length) || canonical block bytes`
for every height in order, cut into `archivePieceBytes` pieces (the last may
be shorter). Pieces are the leaves of an ordered, count-committed v1 Merkle
tree (ADR 0008 construction, kind `archive`). The commitment records chain
id, segment index, first and last height, piece size, total bytes, piece
count, piece root and the last block hash, and
`segmentId = H("ARCHIVE/SEGMENT-ID", commitment)`. Every node computes
commitments while it still holds the blocks; checkpoints commit the ordered
list (kind `archive-index`), so a pruned node keeps 32 bytes per segment and
can still verify archival proofs.

## Proof of Archival

Proof of Archival is a probabilistic **spot-check of a provider's ability to
return sampled pieces within a deadline**. It is not a formal proof of
retrievability or of exclusive physical storage: this construction has no
extractability proof and does not establish independent physical replicas.
The stronger formal meaning of retrievability requires an extraction
argument; see [Bowers, Juels and Oprea, *Proofs of Retrievability: Theory and
Implementation*](https://eprint.iacr.org/2008/175.pdf). Nodo does not claim
that theorem for this spot-check protocol.

Its narrower guarantee is:

- if a provider has a fixed subset containing a fraction `f` of a segment's
  pieces, a uniformly sampled `k`-piece challenge passes with probability at
  most `f^k`; this bound does not apply when it can obtain missing pieces
  during the response window;
- a provider can still fetch the data from someone else within the
  deadline (outsourcing). Short deadlines raise the cost; replica encoding
  (proof of replication) is future work;
- a provider cannot prove a negative, and nobody can prove that a provider
  did *not* answer: a missing answer may be censorship or an outage.

### Providers and assignment

A provider registers a signed intent: Ed25519 key (its only identity, no
personal data), an **operator id** (the economic identity posting the bond),
a bond and a declared capacity. Bond bounds slots:
`slots <= bond / archiveMinBondPerSlotRawUnits`. Registrations become active
after `archiveProviderActivationDelayBlocks`; the assignment key is
`H(providerId, hash of the block finalized at activation)`, unknown at
registration time, so keys cannot be ground to land on a chosen segment.

The network assigns `archiveReplicationTarget` slots per sealed segment by
rendezvous hashing over active providers, respecting capacity and bond, and
never giving two slots of one segment to the same provider **or operator**.
Rendezvous hashing keeps assignments stable as providers join or leave.

The current reference registry does not escrow the claimed bond or bind the
claimed operator id to canonical economic state. These anti-Sybil rules only
become enforceable after the on-chain activation in roadmap 6.9; the reference
registry must not be used to authorize rewards or pruning today.

### Challenges and proofs

Round `r` is issued at height `r*I+1` and seeded by the block finalized at
`r*I` (`I = archiveChallengeIntervalBlocks`); proofs are due by
`issue + archiveChallengeResponseBlocks <= next round`.

```text
seed    = H(chainId, r, seedBlockHash, providerId, segmentIndex, segmentId)
sample  = first k distinct H(seed, j, attempt) draws below the largest
          multiple of pieceCount under 2^64, reduced mod pieceCount
```

Anyone recomputes a challenge from finalized data; a challenge with chosen
samples is invalid. The seed-block proposer can still grind candidate blocks.
For a fixed held fraction `f`, at most `G` independent candidate seeds and
uniform samples, a union bound is `min(1, G * f^k)`; it is not negligible when
`f` is close to one or `G` is large. These assumptions also exclude rapid
outsourcing. The final challenge sample count and seed source need adversarial
analysis before activation; a beacon or delayed multi-block seed is a future
upgrade.

A proof carries the sampled pieces, their inclusion paths and the provider's
signature (signing domain `NODO_ARCHIVAL_PROOF_V1`); its id excludes the
signature. Verification costs `k` piece hashes, `k` Merkle paths and one
signature, and checks, in order: size cap, provider registered and active,
challenge binding, signature, finalized inclusion height and deadline window,
replay (each challenge is
answered once; a proof can never answer another round), sample set, then
each piece against the segment root.

### Outcomes, score and penalties

| Outcome | Meaning | Consequence |
| --- | --- | --- |
| `PASSED` | Valid proof within the window | Counts toward availability |
| `MISSED` | No valid proof by the deadline | Lowers availability and reward. **Never evidence.** |
| `FRAUD` | The provider **signed** an invalid answer to its own valid challenge | Verifiable `ArchivalFaultEvidence`; immediate removal; the only basis for a bond penalty (`archiveFraudPenaltyBasisPoints`) |

Availability is `passed / issued` per slot and epoch. An epoch with
availability under `archiveMinAvailabilityBasisPoints`, or with fraud, fails.
Reliability ramps from `archiveReliabilityFloorBasisPoints` to 1.0 over
`archiveReliabilityRampEpochs` consecutive good epochs and drops to the
floor after a failed one. `archiveRemovalFailedEpochs` consecutive failures
remove a provider. Bad signatures, wrong challenges, late and duplicate
messages change nothing: an unauthenticated or replayed message is not a fact
about a provider. **No penalty without verifiable evidence.**

### Replication

Replication counts **distinct operators that proved the segment** in the
epoch, never claimed or assigned identities. Segments below the target are
under-replicated; segments at or below `max(1, minProvenReplicasBeforePrune)`
are critical. Bytes are accounted as history bytes, proven replica bytes
and target replica bytes; prices are left to governance and never
hard-coded.

### Rewards

Archival rewards are part of the Epoch Protection Budget, bounded by
`EpochEmissionPolicy`:

```text
Epoch Protection Budget (= epoch emission cap)
  Consensus      remainder, at least 60%
  Availability   0 until a verifiable metric exists
  Data protection 0 until challenge settlement exists
  Archival       archiveRewardShareBasisPoints (protocol cap 30%)
```

```text
weight(slot) = bytes(segment) x scarcity(proven replicas)
               x availability(passed/issued) x reliability
target       = sum(bytes) x replicationTarget          (all multipliers 1.0)
reward       = archivalBudget x weight / max(target, sum(weights))
```

Below the target, every weighted byte earns the same rate, so a provider's
reward does not shrink when others join; scarce segments pay more per byte
(scarcity tiers: 1.0x at the target, rising as proven replicas fall, at most
5.0x). Above the target the budget is shared pro rata. A slot with no passed
challenge, availability under the floor or fraud earns nothing. The sum of
rewards never exceeds the budget and the remainder is **not minted**.
Arithmetic is integer-only and deterministic; `ArchivalRewardSchedule::audit`
recomputes a settlement for the supply audit. **No reward without measurable
protection work.**

### Sybil resistance

Ten thousand identities do not earn ten thousand rewards:

- slots need bond, so extra identities need extra capital and earn nothing
  per unit of capital;
- assignment is by the network, not the provider, and an operator never
  holds two replicas of one segment;
- replication and scarcity count proven distinct operators;
- an attacker with a fraction `α` of bonded capacity fills all `R` slots of
  a segment with probability about `α^R`, and even then the segment still
  has its one real copy.

Economically independent operators that collude, and one machine answering
for several independent operators, are not detectable by this protocol;
replica encoding is the long-term answer. Operators are never asked for
real-world identity; an optional coarse network-diversity signal must stay
privacy preserving.

### Serving data

Serving snapshots, ranges and recovery help is valuable but not provable
yet: a served byte cannot be distinguished from a fabricated claim without
the receiver's cooperation. It earns nothing in this version
(`SERVE_HISTORICAL_BLOCK` maps to the unbudgeted availability pillar).

## Wire messages

Fifteen typed messages (`HistorySyncMessages`): checkpoint, snapshot
manifest and chunk requests and responses, history ranges (1-4 blocks),
archive index and replication status, provider and segment announcements,
challenges and proofs. Every message has a u16 tag, strict canonical
decoding, bounded counts and byte lengths (4 MiB payload, 256 KiB chunks,
1024 index entries), names data only by height, segment or 64-hex id (never
by path), and embeds protocol objects as bounded bytes verified by their
own codecs. A per-peer guard accepts only responses to open requests of the
expected type within a timeout, caps outstanding requests and tracked peers,
and charges served bytes per window against amplification.

## Storage layout and migration

Storage schema **v2** adds:

```text
history/checkpoints/<height>.checkpoint
history/checkpoints/conflicts/<height>-<id>.checkpoint
history/snapshots/<height>.snapshot
history/archive/commitments/<segment>.segment
history/archive/self_audit.nodo
history/pruning/manifest.nodo
history/pruning/journal.nodo
```

Every write is atomic, every read is size-capped and refuses symlinks, and
names derive only from heights and indices. Version 1 directories still
load; history writes require the explicit, idempotent `nodo storage
migrate`, which creates the tree, converts the legacy pruning manifest and
rewrites the schema file last. Older binaries refuse a v2 directory.

## Parameters

`config::HistoryParameters` centralizes every value per network; there are
no magic numbers elsewhere. Network-wide values are committed by
`deterministicId()`; node-local safety margins are not.

| Parameter | localnet | localnet-soak | testnet-candidate |
| --- | --- | --- | --- |
| `checkpointIntervalBlocks` | 8 | 64 | 43200 |
| `checkpointConfirmationBlocks` | 2 | 8 | 600 |
| `weakSubjectivitySeconds` | 14 days | 14 days | 14 days |
| `minimumPruningRetentionBlocks` | 16 | 128 | 86400 |
| `minimumRetainedCheckpointSnapshots` | 2 | 2 | 3 |
| `snapshotChunkBytes` | 16 KiB | 64 KiB | 256 KiB |
| `archiveSegmentBlocks` | 4 | 16 | 43200 |
| `archivePieceBytes` | 512 | 4 KiB | 64 KiB |
| `archiveChallengeIntervalBlocks` | 4 | 16 | 4320 |
| `archiveChallengeResponseBlocks` | 2 | 8 | 720 |
| `archiveChallengeSamples` | 4 | 8 | 32 |
| `archiveReplicationTarget` | 3 | 3 | 5 |
| `archiveMinProvenReplicasBeforePrune` | 0 | 0 | 3 |
| `archiveRewardShareBasisPoints` | 1000 | 1000 | 1000 |
| `archiveMinAvailabilityBasisPoints` | 8000 | 8000 | 9000 |

Mainnet has no profile until Phase 10 locks it.

## Threats and mitigations

| Threat | Mitigation |
| --- | --- |
| False checkpoint | Id must equal the out-of-band anchor; identity, schedule, link and QC verification |
| Invalid QC | Canonical quorum, exact block and parent, signatures and weight against the committed set |
| Conflicting checkpoints | Never overwritten; both preserved as evidence of a safety fault |
| False or poisoned snapshot | Chunk digest, recomputed state root, strict domain decoding, window and projection checks |
| Long-range history / eclipse at bootstrap | Weak-subjectivity anchor no older than 14 days; peers can only withhold, never forge |
| Gigantic snapshot, memory exhaustion | Size caps before decoding, chunk-by-chunk verification, memory grows with verified bytes only |
| Decompression bomb | Nothing is compressed or decompressed |
| Disk exhaustion | Bounded checkpoint snapshots, bounded legacy snapshot cache, size-capped reads |
| Path traversal | Names from integers only; pruning allow-list; journal paths re-validated; symlinks refused |
| Crash during pruning | PREPARE/DELETE/COMMIT journal, roll-forward recovery, quarantine on tampering |
| Partial corruption | Strict canonical decoding everywhere; `nodo checkpoint verify`; self-audit detects segment drift |
| Integer overflow | Checked arithmetic, 128-bit weights with deterministic down-scaling |
| Fake provider / fake proof | Signed registration, bond, delayed activation; Merkle and signature checks |
| Challenge grinding | Seed from a finalized block after assignment; bound `G*f^k` |
| Replayed or old-epoch proof | One answer per challenge id; deadline window; round-bound challenge ids |
| Duplicated proof | Replay guard; duplicates change nothing |
| Malformed segment | Strict commitment codec; geometry must match parameters |
| Sybil providers | Bond per slot, operator diversity, proven-operator replication |
| Under-replication | Network assignment, scarcity multipliers, critical-segment pruning floor |
| Withholding | Missed challenges lower score and reward and eventually remove; never slashed without evidence |

## Consequences and gates

The BFT consensus and its quorum certificates remain the only source of
finality; Proof of Archival protects history and never votes. Remaining
gates, all in the roadmap: checkpoint-base reload and committed validator
scores before block bodies are deleted (3.17); on-chain archival
transactions, reward minting and supply accounting through a protocol
upgrade, because the v1 registry of ADR 0011 is closed (6.9); live P2P
wiring of the history messages (5.9); weak-subjectivity checkpoint
distribution and authenticated validator-set transitions (5.3, 5.6).
