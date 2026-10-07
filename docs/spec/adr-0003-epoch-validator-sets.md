# ADR 0003: Epoch-bound validator sets and bounded weight churn

**Status:** accepted for the development protocol `nodo/0.4` and the v1
design contract. The v1 binary commitment and proof format remain Phase 2
and Phase 5 work.

## Decision

Genesis is height 0. For the current fixed epoch length `L = 43200`, epoch
`e` (zero based) votes at heights `[eL+1, (e+1)L]`. The genesis validator set
votes in epoch 0. Only the finalization of height `(e+1)L` may select the set
for height `(e+1)L+1`; a transaction, slash, exit, top-up or rotation in any
other block changes economic state but not the set authorized to sign votes or
propose blocks in that epoch. A QC at height `h` always uses the immutable set
selected for `h`, including during replay and artifact import. The historical
set root and checked total weight are part of the QC audit.

Validator registration and new stake become eligible no earlier than epoch
`e+2` for a transaction in epoch `e`. The first possible voting height is
`(e+2)L+1`, after two finalized epoch boundaries. A key rotation cannot
request an earlier activation epoch. Finalization derives the next set from
the authenticated finalized state, never from unfinalized gossip or local
wall time.

The development profile fixes the maximum changed voting weight per epoch
at `floor(previous_total_weight * 3333 / 10000)`. Churn is the sum over the
union of validator IDs of `abs(previous_weight - next_weight)`. The arithmetic
uses a wide intermediate to avoid overflow. This is a weight budget, not a
count of keys: splitting stake among many IDs does not create extra budget.
There is no top-K selection or per-validator weight cap. Decreases and
removals are processed first, then other changes in validator-ID order.
Changes beyond the remaining budget stay staged; a large weight change can
advance in raw-unit increments, and a new voting identity enters only with
at least the minimum stake. A replacement key waits until its old key is no
longer live in the selected set, preventing double assignment of its stake.
Pending, jailed and exited registry entries are omitted from consensus
snapshots. This keeps inactive identities out of QC set roots and bounds the
active set by voting weight divided by minimum stake, without a Sybil-prone
top-K cap. The selected set must retain positive total voting weight.

`ValidatorSetHistory` covers every verified height but stores a full registry
only at a change point. A lookup for an intermediate height returns the last
snapshot at or before it; it cannot use a later registry. This avoids a full
registry copy per block while retaining exact historical QC verification.

## Security boundary

The transition is deterministic and bounded by signed voting weight. It does
not establish that every old vote remains backed by locked stake while an
exit or weight reduction is waiting for its churn slot. The short unbonding
window (roadmap 1.5), one-to-one genesis stake backing (3.16), formal proof of
dynamic-set safety, authenticated light-client transition proofs, and the
full v1 evidence-driven jail exception remain prerequisites for production.
The Byzantine bound of [ADR 0002](adr-0002-fault-model-and-quorum.md) must hold
for each selected set; an epoch boundary cannot repair an already violated
fault assumption.
