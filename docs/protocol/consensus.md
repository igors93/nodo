# Consensus

Nodo's consensus foundation separates vote and round logic from transport and economics.

## Implemented Foundations

- validator vote records;
- quorum-safe math;
- consensus round manager;
- proposer schedule;
- network vote collector;
- round timeout foundations;
- block finalizer;
- fork choice foundations;
- slashing evidence for conflicting votes and proposer equivocation;
- finalized slashing evidence applies deterministic validator penalties, registry jail/tombstone effects and bounded staking slash effects.

## Design Boundaries

- Consensus should not depend directly on TCP transport.
- Consensus should not implement treasury or governance economics.
- Votes must be checked against height, round, validator identity, and block target.
- Duplicate votes, conflicting votes and same-round proposer equivocation must be explicit and slashable.

## Status

The repository has strong local foundations, but it is not a complete production BFT network.
