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
- testnet readiness and diagnostics foundations;
- gossip transaction deduplication via `SeenTransactionCache` (LRU + TTL) —
  prevents amplification from peers replaying the same payload;
- gossip payload signature verification before mempool admission — no unsigned
  or wrongly-signed transaction is admitted from the network;
- `ProductionKeySafetyGate` blocks localnet-only keys on official networks;
- `LOCKED_PRODUCTION` (mainnet) startup is refused at the CLI level;
- genesis compatibility check prevents mixing data directories across networks;
- finalized artifact QC verification against the local validator registry before
  recording — no finalized block is accepted on peer word alone;
- no implicit trust in P2P messages: every consensus payload is verified
  locally before acting on it.

## Current Limits

- local development keys are not production custody;
- testnet-candidate is not a public production network;
- mainnet is blocked;
- P2P foundations are not yet a fully hardened production networking stack;
- external audits have not been claimed.

## Operator Rule

Treat all current builds as development or pre-testnet software unless a future signed release explicitly says otherwise.
