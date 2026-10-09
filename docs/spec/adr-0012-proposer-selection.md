# ADR 0012: Stake-weighted proposer priority and round fallback

**Status:** accepted Phase 1 design decision for the future v1 protocol.
`nodo/0.7` still uses a development hash lottery; Phase 4.8 must replace it
on every production, validation, replay, sync and import path before v1 can
be advertised. The checked `V1WeightedProposerSchedule` is a reference, not
a live-protocol switch.

## Decision

V1 uses deterministic weighted proposer priority. There is no hash lottery,
random ticket, modulo reduction of a digest, VRF seed, or unverified
proposer-supplied priority. Weight is the linear locked stake of the frozen
validator set for the height. The [CometBFT proposer-selection
specification](https://github.com/cometbft/cometbft/blob/main/spec/consensus/proposer-selection.md)
motivates the add-weight, select-maximum, subtract-total construction; the
rules below fix Nodo's own height and fallback semantics completely.

At genesis, the proposer-priority vector has one signed `i128` zero per
active validator, sorted by validator ID. Its value and the genesis
validator-set root are part of the initial state. At positive height `h`,
the prior finalized state contains exactly one priority entry for every
validator in the frozen active set for `h`, no extra entries, and the same
verified `validator_set_root` and checked positive total weight `W`. A missing,
duplicate or mismatched priority makes the candidate invalid. Every integer
addition, subtraction and sum is checked. `i128` uses fixed 16-byte,
big-endian two's-complement encoding. The canonical vector is
`set_root:hash, last_finalized_height:u64, total_weight:u64,
entries:list<proposer_entry>`, where an
entry is `validator_id:validator, priority:i128` inside a `u32` byte-length
prefix of exactly 48; IDs are strictly sorted.
The 1 MiB validator-set cap bounds this list to 9619 entries in v1.

For each active validator `i`, compute `score_i = priority_i + weight_i`
using the prior finalized vector. Sort all validators by descending score,
breaking ties by ascending 32-byte validator ID. Rounds are zero based. At
round `r:u64`, the sole valid proposer is sorted entry `r mod n`, where `n`
is the positive active-validator count. This remainder cycles through an
already ordered list; it is not random sampling and has no modulo lottery
bias. In every `n` consecutive rounds, each active validator has exactly one
proposer opportunity. A block with any other header proposer or consensus
proposal signer is invalid. An honest node MUST never sign a proposal for a
round assigned to another validator.

After finalization at height `h`, advance the priority vector exactly once,
independent of which round finalized: set every priority to its `score_i`,
then subtract `W` from the **round-zero primary's** priority. Do not
subtract from the eventual backup proposer and do not advance once per
failed round. Thus a validator cannot manipulate future primary allocation
by delaying finality to a chosen round. The advanced vector has the same
checked sum as the prior vector (zero at genesis and after every valid set
transition). An unfinalized height never changes persisted priorities.

The last block of an epoch uses the old set and advances its priorities as
above. If it commits a changed set for the next height, remove exited
entries; carry priorities for IDs that remain; initialize each new ID to
`min(carried_priorities) - W_next`, or zero when no ID remains. This penalty
prevents an exit/rejoin or split identity from gaining an immediate priority
reset advantage. No set change or rebase is legal at a non-boundary height.

Normalize the resulting vector after **every** finalized height, whether
the set changed or not, against the next set's total weight `W_next`. If
`range = max(priority)-min(priority)` exceeds `2*W_next`, divide every
priority by `ceil(range/(2*W_next))`, rounding toward zero. Subtract the
signed, truncation-toward-zero average from each entry. If the residual sum
is positive, subtract one from that many lowest-ID entries; if negative,
add one to that many lowest-ID entries. The resulting sum is zero and range
is at most `2*W_next+1` after integer rounding. With positive checked
`u64` total weight and at most 9619 entries, this cap keeps the next score
and sum inside signed `i128`; all arithmetic is still checked. A set
unchanged at the boundary gets no set-change reinitialization. The next
block uses the committed new set and normalized vector. Historical replay
must use the vector as it existed at that height,
never the latest one.

State domain 12 stores this single proposer-schedule value under a 32-byte
zero key. Genesis sets `last_finalized_height=0`; finalizing height `h`
sets it to `h`. Thus even a one-validator schedule changes on every
finalized height. System-record tag 9 is
`proposer_schedule`; its `affected_id = H("PROPOSER-SCHEDULE", empty)`,
`previous_value_hash = H("STATE-VALUE", previous_exact_value_bytes)`, and
`new_value:bytes` contains exactly the 32-byte
`H("STATE-VALUE", next_exact_value_bytes)` commitment. The complete next
value is in authenticated state and must be independently recomputed; the
compact record is not authority to choose a value. This fixed mandatory
transition has one zero-fee receipt with `effect_ids` containing only its
`affected_id`. It consumes reserved system resources under ADR 0009 even if
the block finalized in a later round. Snapshots must contain the full domain-12
leaf and prove it against the finalized state root. Light clients can verify
the proposer sequence from a recent authenticated checkpoint containing this
leaf and the frozen set; a header or peer STATUS alone is insufficient.

For a stable set, the round-zero sequence follows weighted priority rather
than independent lotteries. A validator's voting weight controls the rate
at which its credit grows; splitting one stake across IDs does not increase
aggregate stake or grant extra round-zero weight. This rule does not promise
that finalized blocks are distributed exactly by stake when adversaries
force later rounds. Such manipulation must not affect the next primary,
which is why finalization always subtracts from the round-zero primary.
The first `n` round slots give a direct proposer-opportunity bound under a
frozen set, subject to the partial-synchrony and Byzantine-weight assumptions
in ADR 0002. Fairness across dynamic-set transitions and lock/view-change
liveness remain formal-model and external-review obligations in Phase 1.

The schedule is public and future primaries are predictable while set weights
remain unchanged. Weighted round-robin is chosen for deterministic fairness
and bounded round fallback; it does not claim secrecy or prevent targeted
network attacks. Validators need redundant networking, private sentry paths,
peer rate limits and DDoS response as operational defenses. A later VRF or
threshold randomness design would require an explicit protocol upgrade with
unbiasable entropy, verifiable proofs and its own liveness analysis; a hash
of the preceding block or QC is not an acceptable substitute.
