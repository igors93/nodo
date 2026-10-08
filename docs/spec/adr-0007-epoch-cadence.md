# ADR 0007: Genesis-bound height epochs and BFT block cadence

**Status:** accepted Phase 1 decision. The v1 rule is specified and has an
executable checked reference. The development runtime still has separate,
hard-coded validator and issuance windows; migration is tracked in Phase 4
and Phase 6 of the roadmap.

## Decision

V1 uses **height-based consensus epochs**. Immutable genesis parameters are
`epoch_length_blocks = L` and `target_block_seconds = T`. Both are positive;
`T <= 300` and the checked product `86400 <= L*T <= 604800` (one to
seven nominal days).
Genesis height 0 has no epoch. For every positive height `h`, the zero-based
epoch is `floor((h-1)/L)`. Epoch `e` includes heights `e*L+1` through
`(e+1)*L`, inclusive. The latter is its only boundary block. Every height,
epoch, boundary, and activation calculation uses checked `u64` arithmetic;
overflow makes the value invalid instead of wrapping. The validator and
parameter sets change only after the boundary is finalized, starting at
height `(e+1)*L+1`. The boundary block is validated under the old set.

The exact BFT-time rule is [ADR 0006](adr-0006-bft-time.md). It enforces
`child.time >= parent.time + T`, with checked signed arithmetic and a verified
parent QC for heights above 1. Honest validators also wait at least `T` on a
local monotonic clock after finalizing a height before signing at the next
height. Therefore `genesis.time + h*T` is a checked **lower bound** for a
height's header time, not its required timestamp. The nominal minimum between
consecutive epoch boundaries (or genesis and the first boundary) is `L*T`;
delayed finality, partitions and QC vote times can make that BFT-time interval
longer. No wall-clock instant advances an epoch,
skips an epoch, or grants a validator set or economic transition by itself.

The genesis issuance schedule is expressed in exact units per epoch. Epoch
`e` issues its scheduled units once, in its last finalized block `(e+1)*L`;
epoch 0 issues zero outside genesis. An epoch without a schedule entry issues
zero. A stalled chain neither mints the missed epochs in bulk nor
converts elapsed wall time into extra issuance. V1 has no `epochs_per_year`,
`annual_blocks`, or independent `epoch_duration_seconds` consensus parameter.
Any policy advertised as a calendar-year rate must be separately specified
and proved against BFT time and the enforced cadence; Phase 6.2 owns that
work. Evidence, unbonding and weak-subjectivity periods specified in seconds
use authenticated BFT header time. Governance and activation delays specified
in epochs use height-derived epoch indices. Implementations never silently
convert between these units using an assumed number of seconds per block.

## Rationale and implementation boundary

Height-defined epochs let validators compute membership from finalized chain
data even when the network is partitioned or clocks differ. Binding both `L`
and `T` in genesis prevents nodes from disagreeing on boundaries or silently
changing emission speed. The one-day minimum prevents validator sets from
changing every few blocks; the seven-day maximum bounds how long an
otherwise healthy chain waits to apply a set update. It does
not promise finality during a partition. The `EpochCadence` reference checks
parameter limits, boundaries, epoch indices, height overflow and the BFT-time
lower bound. Tests use different `L,T` pairs and boundary/overflow cases.

The current `nodo/0.7` runtime is not v1. Its unused
`NetworkParameters::epochDurationSeconds`, 43,200-block validator windows,
525,600-block issuance windows and 365-epoch emission divisor do not follow
this decision. Phase 4 must read `L,T` from authenticated genesis, use one
cadence implementation in production, voting, finalization, replay, import,
validator scheduling and governance, and enforce the time rule. Phase 6 must
replace all independent height constants, annualized development assumptions
and economic window conversions before public testnet. Until then the
development runtime must not claim a production-safe cadence or annual rate.
