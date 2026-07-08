# Roadmap

The roadmap is organized by maturity tracks instead of strict historical phases. Older cycle notes were removed from the canonical documentation because they mixed completed implementation history with future requirements.

## Completed foundations

### Localnet and storage

- Local node initialization.
- Local key creation for development.
- Transaction submission and block production.
- Finalized block persistence.
- Manifest and storage schema validation.
- Runtime reload from persisted artifacts.
- Chain audit command.
- Persistent mempool foundations.
- Atomic file writes for critical data.

### Consensus and networking

- Weighted validator vote model.
- PREVOTE/PRECOMMIT voting flow.
- Quorum certificate generation from valid PRECOMMIT votes.
- Proposer schedule foundation.
- Timeout/view-change foundation.
- Consensus recovery store foundation.
- TCP transport and gossip mesh.
- Peer authentication, peer exchange, reconnect policy, rate limiting, temporary bans, quarantine, and eclipse-resistance foundations.

### Economics and validator accountability

- Coin lot model and registry foundations.
- Ledger record integration.
- Explicit transaction coin-lot inputs.
- Stake lifecycle foundations.
- Validator weight projection from active stake.
- Epoch reward settlement foundation.
- Validator score and network-risk concepts.
- Slashing evidence and deterministic penalty records.

### Governance, treasury and public API

- Governance proposal submission.
- Vote evidence.
- Vote-set audit.
- Decision and execution foundations.
- Lifecycle audit.
- Treasury policy checks.
- Treasury execution evidence.
- JSON-RPC public API foundation.
- Health and metrics endpoints.

### Key management foundations

- Encrypted local key files.
- Key rotation foundation.
- External signer/HSM boundary foundation.
- Development diagnostics for unsafe key usage.

## Active hardening tracks

### Track 1 — Protocol specification

Required outcome: a stable protocol specification that separates consensus, Proof of Protection economics, state transition, networking, and audit rules.

Work items:

- define formal consensus assumptions;
- specify validator-set snapshots and epoch boundaries;
- define all canonical state roots and receipts roots;
- document slashing evidence schemas and replay rules;
- document state-transition rejection reasons.

### Track 2 — Economics and Proof of Protection

Required outcome: a testnet-ready economic model with explicit limits and deterministic reward rules.

Work items:

- lock testnet monetary parameters;
- define reward caps per epoch;
- define measurable protection work;
- prevent self-generated fake work from earning rewards;
- define validator score as a deterministic audit output, not a subjective trust score;
- finalize stake weighting, minimum stake, unbonding, and penalty rules.

### Track 3 — Storage, pruning and fast sync

Required outcome: nodes can sync, prune, restart, and audit without accepting unverifiable state.

Work items:

- harden snapshot format;
- document snapshot trust boundaries;
- expand storage audit coverage;
- validate fast-sync paths against canonical checkpoints;
- prove pruned nodes retain enough data for required audit modes.

### Track 4 — Public testnet operations

Required outcome: independent operators can join a public testnet safely.

Work items:

- publish testnet genesis;
- create validator onboarding guide;
- publish network parameters;
- define minimum hardware and network requirements;
- publish monitoring and alerting runbook;
- run a multi-validator soak test;
- document incident response and recovery.

### Track 5 — Production custody

Required outcome: validators can run without unsafe local hot-key assumptions.

Work items:

- complete external signer/HSM workflow;
- document key rotation and revocation;
- document backup and recovery;
- reject unsafe key paths on production networks;
- test validator operation with remote signer boundary.

## Mainnet readiness gate

Mainnet can only be considered after public testnet success and external review. Minimum requirements:

- external security audit;
- testnet with independent validators;
- stable economic parameters;
- production key custody workflow;
- documented governance process;
- documented treasury emergency process;
- deterministic replay/audit validation;
- release and rollback procedure;
- disaster recovery procedure;
- explicit final approval checklist.

## Non-goals until ready

Nodo should not claim or enable the following before the readiness gate:

- production mainnet;
- real treasury value;
- custody of user funds;
- permissionless public validator onboarding without safety gates;
- final monetary policy claims;
- final legal/compliance claims;
- irreversible governance execution on a live-value network.
