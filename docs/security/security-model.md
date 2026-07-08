# Security Model

Nodo treats security as a protocol requirement, not an optional operational add-on.

## Current protections

Implemented or foundation-level protections include:

- canonical serialization checks;
- strict storage schema validation;
- deterministic state-transition replay;
- signature verification;
- BLS validator signatures and Ed25519 user/peer signatures;
- quorum certificate validation;
- finalized state-root verification;
- atomic persistence;
- peer authentication foundations;
- rate limiting;
- temporary peer bans and quarantine;
- eclipse-resistance foundations;
- governance vote evidence;
- treasury execution evidence;
- slashing evidence;
- key safety diagnostics;
- health and metrics endpoints.

## Security assumptions

Nodo should assume:

- peers may be malicious;
- persisted files may be corrupted or tampered with;
- RPC clients may send malformed or oversized requests;
- validators may equivocate;
- operators may misconfigure nodes;
- old data may be replayed;
- local development keys are unsafe for production.

## Fail-safe preference

If Nodo cannot prove that input is safe and canonical, it should reject that input or halt the unsafe path.

## Current limits

The following remain incomplete for production use:

- external security audit;
- production custody workflow;
- public validator onboarding process;
- formal consensus specification;
- final economic abuse analysis;
- public incident-response runbook;
- mainnet release procedure.
