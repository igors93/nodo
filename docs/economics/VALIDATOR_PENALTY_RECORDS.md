# Nodo Validator Penalty Records

Status: Implementation Guide  
Version: NODO-VALIDATOR-PENALTY-RECORDS-V1

## Purpose

This phase turns double-sign evidence into auditable penalty records.

In simple terms:

```text
validator signs two different blocks for the same height
        ↓
ValidatorDoubleSignEvidence
        ↓
ValidatorPenaltyRecord
        ↓
ValidatorScoreRecord
        ↓
LedgerRecord
```

## New Components

```text
ValidatorPenaltyRecord
ValidatorPenaltyPolicy
```

## Why This Matters

The previous phase could detect double-signing.

This phase makes that detection actionable and auditable by writing a penalty into the ledger.

## Security Rules

The implementation rejects:

```text
invalid double-sign evidence
zero epoch
invalid score ranges
zero timestamp
same block hash in both evidence entries
unsafe delimiter characters in critical fields
invalid penalty policy
```

## Penalty Behavior

The first conservative policy reduces validator score by 40 points:

```text
previous score 80 -> new score 40
previous score 30 -> new score 0
```

The penalty is clamped at zero.

## Ledger Order

Penalty output uses canonical order:

```text
1. VALIDATOR_PENALTY
2. VALIDATOR_SCORE
```

The score record points back to the penalty by using the penalty deterministic id as evidence hash.

## Follow-up: Automatic Stake Slashing

Later phases implemented automatic penalty application to locked stake:
`node::CanonicalSlashingTransition` converts finalized evidence into one
deterministic penalty decision, deducts a bounded fraction of locked stake in
the staking registry and emits auditable `StakePenaltyRecord` entries in the
finalized artifact. See
[Validator Penalty Application](../VALIDATOR_PENALTY_APPLICATION.md).

Still not implemented:

```text
appeal / review flow
```

## New Test

```text
tests/economics/ValidatorPenaltyRecordTests.cpp
```
