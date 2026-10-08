# ADR 0006: Authenticated BFT time and local clock boundaries

**Status:** accepted Phase 1 decision. `nodo/0.7` implements the checked
reference calculation and rejects PRECOMMIT votes timestamped before their
block. Binding every produced and imported block to the calculated v1 time
remains an explicit Phase 4 gate.

## Decision

Consensus time is the `time:i64` in a finalized block's authenticated header,
in positive Unix seconds. It is neither the proposer's freely chosen wall
clock nor the time when an observer received or finalized the block.
`target_block_seconds` is a positive immutable genesis parameter. Height 1
uses checked `genesis.time + target_block_seconds`. At later heights the child
contains the hash of one verified parent PRECOMMIT QC and uses
`max(checked(parent.time + target_block_seconds), weighted_lower_median(QC))`.
The child carries that exact QC in its finality artifact. If its QC is
missing, for another block, signed by the wrong historical set, malformed,
or has a vote time earlier than the parent's header time, the child fails
time validation. An integer overflow fails closed.

The median is calculated from all signed PRECOMMIT votes in the selected QC,
using each signer's weight in the frozen validator set for the **parent's**
height. Let `S` be the QC's checked signed weight. After sorting by timestamp,
select the first timestamp whose cumulative weight is at least
`floor(S/2) + (S mod 2)`. For even `S`, a cumulative weight of exactly `S/2`
selects the lower timestamp. The calculation never consults the receiver's
wall clock, the current validator registry or an unsigned weight in a vote.
The verified QC hash in the child header makes a choice between multiple
valid parent QCs explicit; historical replay must retain that QC.

A valid QC has `S > 2W/3`. Under `3B < W`, Byzantine weight `B < S/2`, so
the weighted median remains between the earliest and latest honest signed
vote times even if a Byzantine timestamp itself occupies the median. Honest
clocks must be within the genesis `max_future_skew_seconds` bound (at most 30
seconds) for this to bound the median against real time. A header-time floor
alone does not establish real-time cadence: honest validators must also wait
`target_block_seconds` on a **monotonic** local clock after finalizing a
height before signing at the next height. Timeout durations use monotonic
milliseconds, with round-dependent growth defined by the consensus liveness
rules; they are local scheduling state and are never hashed into blocks.

An honest signer waits until its wall clock reaches the proposed header time
before producing a PRECOMMIT with that current wall-clock time. It persists
signed vote data and the last signed time before broadcast. If the wall
clock moves backward, it waits for it to catch up instead of signing a
contradictory time. Live receivers hold a future proposal or vote in bounded
storage until its time is no more than the configured skew ahead of local
wall time. A peer's clock cannot make an otherwise valid historical block
permanently invalid. Future holding and timeout progression must have
separate byte, count and duration limits to prevent memory exhaustion.

Economic execution uses the authenticated BFT header time for stake unlock,
withdrawal, evidence expiry and every deadline defined in seconds. An
evidence offense at height `h` is aged from the canonical finalized parent
header at `h-1` (genesis for height 1), not a vote timestamp or a reporter's
`detectedAt`. Inclusion is allowed only when the checked difference from that
parent time is in `[0, evidence_max_age_seconds]`; a missing parent or overflow
fails closed. A withdrawal starts its time hold at the unlock block's BFT
time and may execute only at a header time at least the checked deadline,
subject to the longer height/evidence holds. Historical replay uses these
header times and no local clock reads. Weak-subjectivity freshness requires
an authenticated, out-of-band checkpoint and bounded local clock error;
untrusted peer headers cannot establish a fresh trust anchor by themselves.

## Implementation boundary

The `BftTime` reference implementation verifies the parent QC and historical
set before calculating time; regression tests cover weighted and tied
medians, the genesis rule, overflow, wrong QCs/sets and votes before their
block. In `nodo/0.7`, local PRECOMMIT construction and finalized-artifact
matching reject votes earlier than the certified block time. This is an
incompatible development-protocol admission change.

The current development producer still uses a caller-provided block
timestamp and the consensus loop still uses whole-second local round clocks.
Phase 4.8 must enforce the exact BFT time on production, voting, finalization,
replay, sync and import using the QC committed in the new v1 header. Phase
4.6 must replace local timeout scheduling with monotonic milliseconds and
adaptive rounds. Phase 6.3 must migrate every time-based economic rule to
the authenticated header clock. Until those gates are complete, the
development protocol does not claim a production-safe time model.
