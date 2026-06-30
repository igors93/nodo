# Slashing Evidence + Validator Accountability

This phase adds the first slashing evidence and validator-accountability boundary
for Nodo.

It does not apply economic penalties yet. The goal is to make validator
misbehavior provable, durable and shareable before penalty rules are connected to
balances or staking state.

## Added modules

- `consensus::DoubleVoteEvidence`
- `consensus::SlashingEvidenceRecord`
- `consensus::SlashingEvidenceVerifier`
- `consensus::EvidencePool`
- `consensus::ValidatorAccountabilityTracker`
- `storage::SlashingEvidenceStore`
- `node::SlashingEvidenceAnnouncement`
- `node::SlashingEvidenceRequest`

## What works now

A node can:

1. detect two conflicting votes from the same validator at the same height and
   round during live vote admission;
2. return the exact accepted vote + rejected conflicting vote with the
   `REJECTED_CONFLICTING` result;
3. convert that conflict into deterministic evidence immediately in the
   consensus tick that observed it;
4. verify the historical validator set and both signatures before admission;
5. persist evidence records to disk before treating them as known;
6. gossip accepted evidence to peers immediately;
7. reload evidence records after restart;
8. produce validator accountability reports;
9. mark validators with slashable evidence as slashable;
10. announce or request evidence through network message primitives.

## Why penalties are not applied here

Slashing must be conservative. Before burning or locking funds, the protocol must
first prove that evidence is:

- deterministic;
- not duplicated;
- bound to a validator;
- durable across restart;
- auditable by other nodes.

Penalty application should come in the next phase, after this evidence boundary is
stable.

## Immediate admission rule

Conflicting votes are no longer handled as a delayed sweep of `VotePool` state.
`NetworkVoteCollector` returns a `DoubleVoteEvidence` candidate with the
`REJECTED_CONFLICTING` result. `ConsensusEventLoop` then rebuilds the evidence
with the local detection timestamp, verifies it through historical validator-set
checks, submits it to `EvidencePool` and broadcasts the accepted evidence during
the same tick. This keeps the safety path conservative while removing the old
window where a conflict was merely rejected and only later converted to evidence.
