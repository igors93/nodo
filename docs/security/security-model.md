# Security Model

Nodo is defensive by design. It should reject invalid data, preserve evidence, reduce unsafe influence, and allow honest nodes to rebuild state.

## Current Protections

- strict storage schema and key-value parsing;
- atomic writes for important persisted files;
- finalized artifact validation;
- runtime reload and chain audit;
- state-transition preview before finality;
- governance lifecycle verification;
- treasury execution evidence validation;
- slashing evidence and idempotent penalty foundations;
- peer rate limiting and inbound message validation foundations;
- testnet readiness and diagnostics foundations.

## Current Limits

- local development keys are not production custody;
- testnet-candidate is not a public production network;
- mainnet is blocked;
- P2P foundations are not yet a fully hardened production networking stack;
- external audits have not been claimed.

## Operator Rule

Treat all current builds as development or pre-testnet software unless a future signed release explicitly says otherwise.
