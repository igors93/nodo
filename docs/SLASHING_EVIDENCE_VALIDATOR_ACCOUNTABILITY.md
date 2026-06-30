# Slashing Evidence + Validator Accountability

This phase adds the first slashing evidence and validator-accountability boundary
for Nodo.

It does not apply economic penalties yet. The goal is to make validator
misbehavior provable, durable and shareable before penalty rules are connected to
balances or staking state.

## Added modules

- `consensus::DoubleVoteEvidence`
- `consensus::ProposerEquivocationEvidence`
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
   round;
2. detect two different `BLOCK_PROPOSAL` messages signed by the same scheduled
   proposer for the same height and round;
3. convert vote conflicts and proposer equivocation into deterministic evidence
   records;
4. reject duplicate evidence in an in-memory evidence pool;
5. persist evidence records to disk;
6. reload evidence records after restart;
7. produce validator accountability reports;
8. mark validators with slashable evidence as slashable;
9. announce, request and respond with either evidence type through network
   message primitives.

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
