# Nodo

## Implementation: Validator Penalty Records

This phase turns double-sign evidence into ledger-backed validator penalties.

New components:

```text
ValidatorPenaltyRecord
ValidatorPenaltyPolicy
ValidatorPenaltyLedgerBuildResult
ValidatorPenaltyLedgerBuilder
```

New behavior:

```text
double-sign evidence becomes a penalty record
penalty record creates a score reduction record
both records can be written into the blockchain ledger
ChainStateRebuilder audits validator penalty records
```

New test:

```text
tests/economics/ValidatorPenaltyRecordTests.cpp
```

New documentation:

```text
docs/economics/VALIDATOR_PENALTY_RECORDS.md
```
