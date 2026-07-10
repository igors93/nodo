# Current Status

Nodo is in pre-mainnet development. The repository contains a working localnet pipeline, networked node runtime foundations, consensus and finality foundations, governance and treasury evidence foundations, staking and reward foundations, storage/reload hardening, and a large automated test suite.

Nodo should still be treated as experimental software. Production custody, public mainnet activation, economic finalization, and external audit remain blocked by design.

## Status matrix

| Area | Status | Notes |
| --- | --- | --- |
| Localnet runtime | Implemented | Init, key creation, transaction submission, block production, reload, audit, and status inspection exist for development. |
| TCP node runtime | Implemented foundation | `node run` exposes peer networking and JSON-RPC. Public operation still requires runbooks and safety gates. |
| Consensus | Implemented foundation | Weighted BFT-style PREVOTE/PRECOMMIT flow, quorum certificates, proposer selection, timeout/view-change foundations, and QC persistence exist. |
| Finalized storage | Implemented foundation | Finalized block artifacts, manifest updates, state-root validation, storage schema checks, and reload rejection paths exist. |
| P2P networking | Implemented foundation | Gossip, TCP transport, peer authentication, peer exchange, rate limiting, banning/quarantine, reconnect policy, and eclipse-resistance foundations exist. |
| JSON-RPC | Implemented foundation | Public protocol API exists at `POST /rpc`; REST remains operational/diagnostic. |
| Governance | Implemented foundation | Proposal, vote, decision, execution, vote evidence, lifecycle persistence, and audit foundations exist. Public governance process is not final. |
| Treasury | Implemented foundation | Policy checks and execution evidence exist. Real treasury use is not allowed until governance, custody, and operations mature. |
| Staking | Implemented foundation | Stake lifecycle and epoch weight projection exist. Final validator economics are still subject to testnet hardening. |
| Rewards | Implemented foundation | Epoch settlement and reward evidence foundations exist. Final monetary parameters are not locked. |
| Slashing and penalties | Implemented foundation | Evidence-backed penalty records and deterministic penalty application foundations exist. Production slashing policy requires audit. |
| Keys and custody | Development only | Password-encrypted local keys (`TESTNET_SAFE`) are sufficient and enforced for `testnet-candidate`/`testnet`. Production (mainnet) custody requires an external signer/HSM workflow, operator policy, and audit, and remains blocked. See [Key management](security/key-management.md). |
| Fast sync and pruning | Partial/foundation | Pruning, snapshots, and sync-hardening foundations exist but still need broader operational validation. |
| Public testnet | Planned | Requires public genesis, onboarding process, monitoring, runbooks, validator instructions, and soak criteria. |
| Mainnet | Blocked | Must remain blocked until external security audit, custody policy, economic finalization, governance readiness, and operational runbooks are complete. |

## Mainnet safety rule

Mainnet must remain disabled until the project can prove the following:

- validators can run with production-shaped keys and safe custody;
- finalized state can be rebuilt deterministically from genesis and canonical history;
- consensus safety and liveness assumptions are documented and tested;
- treasury execution cannot occur without governance-backed evidence;
- monetary expansion cannot occur outside explicit policy;
- slashing and penalties are evidence-backed and idempotent;
- node operators have runbooks, metrics, alerts, and recovery procedures;
- external review has been completed.
