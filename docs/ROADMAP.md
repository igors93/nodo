# Nodo Roadmap

Nodo is in development and pre-mainnet. This roadmap describes project direction; it is not a production-readiness claim.

## Completed Foundations

- Localnet runtime pipeline: init, keys, transaction submission, block production, reload, status, inspection, and chain audit.
- CMake and CTest build/test structure.
- Storage schema validation and atomic file helpers.
- Runtime state reload and manifest/state-root verification.
- Finalized artifact validation and persistence.
- Monetary reports, supply deltas, and supply audit foundations.
- Treasury policy, spend validation, execution evidence, and finalized treasury audit.
- Governance vote proof, vote evidence, vote-set audit, tally, decision audit, lifecycle record, lifecycle store, and lifecycle-backed treasury approval.
- Consensus round manager, proposer schedule, quorum certificates, finalization, fork choice foundations, and slashing evidence.
- P2P message validation, gossip mesh, loopback/TCP transports, encrypted peer-channel foundations, persistent sync, and peer rate limiting.
- Testnet-candidate network profile, readiness checks, and operator diagnostics foundations.

## In Progress

- Official testnet runtime hardening.
- Governance proposal state machine and lifecycle transitions.
- Production key safety gates and audited signing-provider boundaries.
- Validator reward settlement connected to measurable protection work.
- Stake/slash lifecycle integration and validator eligibility hardening.
- Network hardening, peer policy, and testnet operations.
- Canonical storage/reload audit expansion for all finalized economic and governance records.

## Planned

- Encrypted durable validator key management.
- Wallet and custody boundaries.
- Staking-backed validator economics.
- Public governance workflow and operator tooling.
- Production slashing lifecycle with evidence retention and appeals policy.
- Mainnet readiness gates for custody, networking, storage, economics, governance, and security audit.
- External audit process before any production network claim.

## Explicit Non-Goals Until Ready

- No production mainnet claim.
- No production custody claim.
- No unaudited treasury execution path.
- No governance decision accepted without vote evidence.
- No monetary expansion without canonical authorization.
