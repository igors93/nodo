# Threat Model

This document lists the main threats Nodo should defend against.

## Network threats

- malformed peer messages;
- peer spam;
- transaction flooding;
- vote flooding;
- invalid block gossip;
- eclipse attack;
- peer-table poisoning;
- replayed messages;
- delayed or withheld messages;
- connection-slot exhaustion.

## Consensus threats

- double signing;
- conflicting votes;
- invalid proposals;
- proposer equivocation;
- validator-set confusion;
- stale quorum evidence;
- restart/recovery divergence;
- forged or malformed quorum certificates.

## Storage threats

- corrupted manifest;
- missing finalized block;
- reordered fields;
- duplicate fields;
- storage schema downgrade;
- partial file write;
- tampered runtime snapshot;
- malformed persistent mempool entry.

## Economic threats

- balance without origin;
- fake reward work;
- duplicate reward evidence;
- double spend of coin lots;
- unauthorized emission;
- penalty applied twice;
- treasury spend without valid approval.

## Governance threats

- duplicate votes;
- invalid vote weight;
- expired proposal execution;
- replayed execution;
- forged decision record;
- parameter change outside safe limits.

## Out of scope today

The current repository does not yet claim production defense against nation-state actors, fully permissionless mainnet economics, or real-value custody risk.
