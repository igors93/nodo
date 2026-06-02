# Threat Model

Nodo assumes attackers may try to corrupt state, forge evidence, bypass policy, replay records, or weaken operator safety.

## Threats Considered

- forged transaction signatures;
- malformed finalized artifacts;
- corrupted storage schema or manifest files;
- replayed treasury approvals;
- treasury spends without governance lifecycle context;
- forged governance vote proof;
- duplicate governance votes;
- tally tampering;
- decision proof tampering;
- validator double votes;
- duplicate penalty application;
- peer spam and invalid protocol messages;
- unsafe key use on official networks.

## Defensive Direction

Nodo responds by:

- rejecting non-canonical persistence;
- rebuilding state before trust;
- checking evidence ids for replay;
- binding approvals to governance lifecycle decisions;
- keeping mainnet blocked until safety requirements are met;
- separating transport, consensus, governance, treasury, and economics responsibilities.

## Out of Scope Today

The current repository does not claim to solve production custody, public governance UX, production network operations, external validator onboarding, or audited mainnet economics.
