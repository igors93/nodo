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
   round;
2. convert that conflict into a deterministic evidence record;
3. reject duplicate evidence in an in-memory evidence pool;
4. persist evidence records to disk;
5. reload evidence records after restart;
6. produce validator accountability reports;
7. mark validators with slashable evidence as slashable;
8. announce or request evidence through network message primitives.

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
