# ADR 0013: Verifiable participation and bounded inactivity suspension

**Status:** accepted Phase 1 v1 design decision. The C++ reference checks the
accounting and boundary arithmetic. `nodo/0.x` does not implement this v1
state transition or jail rule; Phase 2/4/6 migration is mandatory.

## What can be proved

The signer list of a finalized PRECOMMIT QC proves that its listed validators
signed that QC. Absence from one QC does **not** prove that an operator was
offline: a vote might have arrived late or been excluded by the QC producer.
Nodo therefore calls the measurement *QC participation*, not cryptographic
proof of downtime. No missing vote, missed proposal, timeout, peer report,
operator score, or manual review can burn stake, tombstone a key, extend
unbonding, or create slashable evidence. The only v1 slashable offenses remain
the conflicting signed votes and proposals in protocol v1 section 3.

Inactivity may cause only a reversible **eligibility suspension** after a
complete epoch of low QC participation. It does not change the set that
signed an old height. Suspended stake stays locked, owned and slashable for
real equivocation evidence; no tokens are destroyed. The validator receives
no QC-participation reward for heights where it is absent from the selected
finalized QC. Rewards and score calculations must use the same authenticated
counts, never local gossip or proposer claims.

## Deterministic window

The window is one height epoch of immutable length `L` from ADR 0007. For
epoch `e`, the candidate at boundary height `b=(e+1)*L` is assessed on the
`L-1` finalized QCs at heights `e*L+1` through `b-1`. The boundary height's
own QC is excluded: it cannot be known while its header and next-set root are
signed. This exclusion is uniform, explicit and prevents a circular state
commitment. The first height of the next epoch consumes but does not count the
old boundary QC. Every other block at height `h>1` consumes its verified
parent QC for `h-1` once; height 1 has no parent QC. The header and artifact
MUST agree on that QC, including its historical set root and full canonical
signer list. A different valid QC for the same block may have a different
signer list; the exact parent QC committed in the child header fixes the
count. A node cannot substitute locally observed votes. A malformed, absent,
duplicate, unordered, foreign, incorrectly weighted or unsigned parent QC
invalidates the child before liveness accounting.

For each active validator ID, record `signed_heights` in the window, and a
common `observed_heights`. Genesis starts with epoch 0 and all counters zero.
Each included signer increments its own count once, and each counted parent
QC increments `observed_heights` once. At boundary `b`, it MUST equal
`L-1`; otherwise the block is invalid. The validator is an inactivity
candidate exactly when `4*signed_heights < 3*(L-1)`; equality passes. Checked
wide integer arithmetic is required. `L` is at most 604800 under ADR 0007.
The candidate list is sorted by 32-byte validator ID. An unfinalized height
or failed round never changes a counter. Genesis and every transition commit
the complete counter vector to state, so replay and snapshot import cannot
silently reset a partly completed window.

The single state-domain-13 leaf has a 32-byte zero key and value
`set_root:hash, last_finalized_height:u64, epoch:u64,
observed_heights:u64, total_weight:u64, entries:list<entry>`. Each entry is
a length-prefixed 48-byte `validator_id:validator, weight:u64,
signed_heights:u64`, strictly sorted and in one-to-one correspondence with the
active set. Set root, IDs, weights and total must match the frozen set for
the *next* height. Before the boundary reset, the old vector is used for the
assessment. The boundary's derived new state resets counters to zero, sets
`epoch=e+1`, and rebases entries onto the committed next set. The old set
root remains available to verify the boundary QC in the first child. A
historical boundary proof needs the old authenticated leaf, the boundary's
verified parent QC and the boundary header/state transition; the reset leaf
alone is not a proof of an old candidate. A snapshot at the boundary must
retain or fetch the authenticated old set and boundary QC before validating
the first child; the new leaf alone cannot validate an old QC.

Every finalized height emits mandatory system-record tag 10 with
`affected_id=H("LIVENESS-WINDOW", empty)`,
`previous_value_hash=H("STATE-VALUE", previous_exact_value_bytes)` and
`new_value:bytes` equal to the 32-byte
`H("STATE-VALUE", next_exact_value_bytes)`. Its zero-fee receipt has exactly
that one effect ID. The next value is independently recomputed and committed
to the state root; the record's hash is not authority to fabricate counters.
At a boundary, assessment reads the old counters before next-set selection;
after selecting that set, the reset vector and tag-10 record are committed
before proposer-priority rebase. The record is mandatory even for height 1 and
the first height of an epoch: `last_finalized_height` changes.

## Consequence and re-entry

At a boundary, first process equivocation-driven jails, then inactivity
candidates in ascending ID order, then ordinary exits and weight reductions,
then additions. An inactivity suspension may be applied only while the
existing `max_epoch_churn_basis_points` weight budget remains and at least
four active validators with positive weight remain; otherwise it is deferred
to a future assessment. The budget is
`floor(W_old * max_epoch_churn_basis_points / 10000)` with a checked wide
product; removing an inactive validator costs its entire old voting weight.
There is no partial jail or rounding up of the budget. It never uses the
evidence-driven jail exception to
bypass churn. Its effect is to mark the validator `JAILED`, omit it from the
set for `b+1`, and set `jailed_until` to at least the checked
`boundary_BFT_time + 86400` seconds, without changing its stake or supply.
An existing later jail deadline is never shortened. Frozen current-height
voting weight and old QCs are unaffected. There is no slash record, evidence
ID or evidence-age rule for inactivity. The validator can request unjail only
after the deadline and regain eligibility after the ordinary two-boundary
activation delay, minimum stake and churn checks. Inactivity counters reset
when a new set starts; they cannot be carried to another validator ID.

Because QC omission can be adversarial, this reversible action still needs
economic simulation and a network attack review before activation. It can
reduce the honest fraction of a next set; as with every set transition, the
`B<W/3` assumption must be evaluated for each selected set. Operators may
use evidence of omitted votes for off-chain incident response, but such
material does not alter consensus validity or slashability. A later design
for stronger absence proof requires its own versioned protocol upgrade.

## Resource and implementation gates

The tag-13 value is at most `68 + 52*9619 = 500256` bytes. The tag-10 record
contains a fixed hash, keeping block-body cost bounded. Because replaying and
hashing the vector is O(active validators), tag-10 units are the normal system
record units plus `128*max(old_set_count,next_set_count)`. At the 9619-entry cap this is
1,232,549 units for a 105-byte record and 124-byte receipt. V1 requires
`max_block_units >= 5,000,000` so the mandatory tag-10 and proposer-schedule
records fit within the reserved system quarter even at maximum membership;
other already-committed work is reserved before optional transactions. A
parameter or set transition that cannot reserve all required work is invalid.

Phase 2.1 must encode the tag-13 leaf and tag-10 record. Phase 4.8 must
derive counters from independently verified parent QCs on production, vote,
replay, import and snapshot paths. Phase 5.3 must prove the leaf at sync
checkpoints. Phase 6.3 must apply bounded suspension and unjail; Phase 6.4
must tie rewards to authenticated participation. The current development
`ValidatorPenaltyRecord` accepts only double-sign score records; the
`MANUAL_REVIEW` reason and score reason are removed. Neither this record nor
an arbitrary score hash is a v1 slashing authorization.

The [Cosmos SDK slashing implementation](https://github.com/cosmos/cosmos-sdk/blob/main/x/slashing/keeper/infractions.go)
is a reference for deterministic signed-block windows, not authority for
Nodo's punishment policy. Nodo deliberately prohibits downtime slashing
because a QC signer list establishes inclusion, not culpable absence.
