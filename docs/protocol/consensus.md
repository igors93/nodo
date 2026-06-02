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
- slashing evidence for conflicting votes;
- validator penalty decision foundations.

## Design Boundaries

- Consensus should not depend directly on TCP transport.
- Consensus should not implement treasury or governance economics.
- Votes must be checked against height, round, validator identity, and block target.
- Duplicate and conflicting votes must be explicit.

## Status

The repository has strong local foundations, but it is not a complete production BFT network.
