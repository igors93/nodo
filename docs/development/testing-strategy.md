# Testing Strategy

Nodo tests should prove that the protocol rejects unsafe behavior and rebuilds valid behavior deterministically.

## Required test categories

- canonical serialization round trips;
- invalid serialization rejection;
- transaction admission;
- state-transition execution;
- state-root mismatch rejection;
- finalized block replay;
- quorum certificate verification;
- consensus recovery;
- storage schema validation;
- manifest corruption rejection;
- persistent mempool reload;
- governance vote evidence;
- treasury policy;
- slashing evidence;
- reward settlement;
- key safety gates;
- peer policy and rate limiting;
- sync and fast-sync behavior;
- readiness diagnostics.

## Regression rule

Every discovered bug should leave a regression test unless the failure is purely environmental.

## Integration rule

When two protocol domains interact, tests should verify the integration boundary. Examples:

- governance decision → treasury execution;
- staking registry → validator weight snapshot;
- slashing evidence → penalty ledger;
- transaction execution → coin-lot registry;
- finalized block → manifest state root.
