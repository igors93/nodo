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
- immediate slashing evidence capture for conflicting votes;
- validator penalty decision foundations.

## Design Boundaries

- Consensus should not depend directly on TCP transport.
- Consensus should not implement treasury or governance economics.
- Votes must be checked against height, round, validator identity, and block target.
- Duplicate votes must be explicit and conflicting votes must carry enough evidence material to be verified, persisted and gossiped immediately.

## Status

The repository has strong local foundations, but it is not a complete production BFT network.


## Conflict-to-evidence boundary

The vote collector does not merely reject a conflicting vote. When a validator
signs two different targets for the same height, round and decision, the
collector returns the original accepted vote plus the rejected vote as
`DoubleVoteEvidence`. The consensus event loop admits that evidence immediately
through the verified slashing-evidence boundary and broadcasts it to peers.
