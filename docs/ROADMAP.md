# Nodo Roadmap

Nodo is in active development. This roadmap is the authoritative implementation
guide for what still needs to be built before the blockchain is minimally
acceptable for a public testnet and, later, mainnet. It is written against the
current codebase — every item references real modules and gaps confirmed by
reading the source.

> **How to read this file:** each phase must be entirely completed before the
> next one begins. Within a phase, items are ordered by dependency — earlier
> items unblock later ones. "Done" means implemented, tested, and the relevant
> docs updated. No exceptions for safety-critical items.

---

## Completed

### Localnet Foundation
- Localnet runtime pipeline: `init`, `keys`, `tx submit`, `block produce`,
  `node reload`, `status`, `inspect`, `chain audit`.
- Storage schema validation and `AtomicFile` crash-safe writes.
- Runtime state reload and manifest / `latestStateRoot` verification.
- Canonical replay through `ProtocolStateTransition`: accounts and protocol
  domains now advance together into one `ProtocolReplayState` for reload, import,
  production context building and manifest verification.
- Finalized artifact codec, store, and multi-domain validator
  (`FinalizedArtifactValidator` covers finality, state, monetary, treasury,
  slashing, governance, and validator lifecycle sections).
- Monetary records, supply delta builder, supply audit, epoch monetary reports,
  and `MonetaryFirewall`.
- Treasury policy, spend validation, execution evidence, and finalized treasury
  audit.
- Governance vote proof, vote evidence, vote-set audit, tally, decision audit,
  lifecycle record, lifecycle store, and `GovernanceLifecycleStateMachine`
  foundations.
- Consensus round manager, proposer schedule, quorum certificates,
  `BlockFinalizer`, fork choice foundations, and `SlashingEvidence`.
- P2P: `GossipMesh`, `LoopbackTransport`, `TcpTransport`, TCP frame codec,
  `NetworkEnvelope`, peer registry, rate limiter, reputation, abuse evidence,
  handshake manager, encrypted peer channel foundations, finalized artifact sync,
  `BlockSyncHandler`, `EclipseGuard` foundation, `DiscoveryService` foundation,
  `InboundMessageValidator` foundation, `PeerReconnectionPolicy`.
- Testnet-candidate network profile, `TestnetReadinessChecker`,
  `ProductionKeySafetyGate`, operator diagnostics foundations.
- `NodeOrchestrator`: transport + peer discovery + block sync + consensus event
  loop + RPC.

### Block Validation and Sync Hardening ✅
- `CoinLotTransactionValidator` wired into `StateTransitionPreview`: TRANSFER
  transactions are validated against a working `CoinLotRegistry` copy; ownership,
  balance, and double-spend within a block are enforced before voting.
- CoinLot registry digest included in `stateRoot` via `coin_lots` domain in
  `StateRootCalculator::calculateProtocolStateRoot`: divergent registry state
  produces a different root and causes `ProtocolCommitment` rejection.
- `importSnapshot` replaced from a silent no-op to an explicit `REJECTED` return;
  snapshot sync requires full runtime hydration and is deferred to Phase 6.
- `planFromRemoteStatus` snapshot-gap routing removed: all height gaps use
  `REQUEST_BLOCKS` unconditionally; the broken snapshot path is never invoked.
- `ConsensusEventLoop` authoritative-context guard: votes are cast only when
  chain id, crypto context, account-state enforcement and the canonical
  protocol-domain executor are all present.

### Phase 1 — Distributed Node Daemon ✅
- 7 new `NetworkMessageType` values: `TRANSACTION_GOSSIP`, `BLOCK_PROPOSAL`,
  `VALIDATOR_VOTE`, `QUORUM_CERTIFICATE`, `FINALIZED_BLOCK_ARTIFACT`,
  `BLOCK_SYNC_REQUEST`, `BLOCK_SYNC_RESPONSE`.
- `SeenTransactionCache` (LRU + TTL) for gossip deduplication.
- `PersistentMempoolStore::serializeForGossip` and `deserializeGossip` with
  local Ed25519 verification, followed by the shared account/domain admission
  policy before insertion in the mempool.
- `NodeDaemon` tick loop: transaction gossip relay, block proposal relay,
  finalized artifact QC verification and recording.
- `NodeOrchestrator` gossip-access methods: `drainGossipInbox`,
  `gossipBroadcast`, `addAndConnectPeer`, `mutableRuntime`.
- `ConsensusEventLoop` processes remote `VALIDATOR_VOTE`, broadcasts
  `FINALIZED_BLOCK_ARTIFACT` after quorum.
- CLI: `node run --network --data-dir --listen --peer --validator-key`;
  mainnet startup rejected; `ProductionKeySafetyGate` enforced.
- Tests: `SeenTransactionCacheTests`, `TransactionGossipTests`,
  `CommandLineNodeRunTests`.

---

## Phase 2 — Live Distributed Consensus

**Goal:** three real nodes can elect a proposer, exchange votes over TCP,
reach quorum, and finalize a block without any local shortcut.

### 2.1 Proposer Selection Wired to Daemon
- `ProposerSchedule::currentProposer(height, round, validatorRegistry)` must
  be called inside `NodeDaemon::tick` to know whether this node is the
  proposer for the current round.
- When the local node is the proposer: build a candidate block from mempool,
  sign it (`ValidatorBlockProposalSignature`), broadcast as `BLOCK_PROPOSAL`.
- When the local node is not the proposer: wait for a `BLOCK_PROPOSAL` from
  the expected proposer; reject proposals from the wrong proposer
  (wrong-proposer rejection test required).

### 2.2 Two-Phase Prevote / Precommit Networked
- `ConsensusEventLoop` currently accumulates votes but the prevote → precommit
  state machine is not driven from real network timing.
- Wire `RoundTimeout` to the daemon tick so that prevote timeout → precommit
  and precommit timeout → next round happen deterministically.
- Duplicate vote detection must reject a second vote from the same validator in
  the same round (test required).
- Conflicting vote slashing: if a validator votes for two different blocks in
  the same round, capture `SlashingEvidence` and broadcast it (test required).

### 2.3 View Change on Proposer Timeout
- If no `BLOCK_PROPOSAL` arrives within the round timeout, advance the round
  counter and rotate the proposer using `ProposerSchedule`.
- The new proposer must re-broadcast a proposal in the new round.
- A node that missed a round must catch up via `BLOCK_SYNC_REQUEST` /
  `BLOCK_SYNC_RESPONSE` rather than stalling.

### 2.4 Consensus Recovery Store Wired ✅
- `ConsensusRecoveryStore` is persisted and reloaded with the active round,
  lock and exact signed PREVOTE/PRECOMMIT records.
- On restart, the event loop reloads the signed votes and resubmits/rebroadcasts
  the same canonical records when the matching proposal is active. Boolean-only
  vote markers are rejected because they protect safety at the cost of liveness.

### 2.5 Integration Tests
- 2-node transaction propagation: node A submits tx, node B receives and admits
  it via gossip.
- 3-validator proposer flow: round 0 proposer broadcasts proposal; all three
  cast prevotes; all three cast precommits; quorum is formed; block finalized.
- Wrong proposer rejection: proposal from non-scheduled proposer is discarded.
- Duplicate vote rejection: second vote from same validator in same round is
  dropped.
- Conflicting vote slashing: two conflicting votes from same validator in same
  round produce a `SlashingEvidence` record.
- Proposer equivocation slashing: two different proposer-signed block proposals
  from the same scheduled proposer at the same height/round produce persisted and
  gossiped proposer-equivocation evidence.
- Canonical penalty effects: finalized double-vote or proposer-equivocation
  evidence now creates a deterministic penalty decision, updates validator
  eligibility and applies bounded stake slashing in the staking registry.
- Quorum formation: 2/3 weight threshold produces a valid `QuorumCertificate`.
- Finalized artifact validation: peer-received `FINALIZED_BLOCK_ARTIFACT`
  verified before recording; malformed artifact silently discarded.
- Peer rejection for wrong genesis / wrong protocol version.

**Exit criteria:** `node run` with three processes on localhost can finalize
a block end-to-end without any in-process shortcut.

---

## Phase 3 — Network Hardening

**Goal:** the P2P layer is robust against common adversarial conditions and
ready for a semi-public testnet with untrusted peers.

### 3.1 Peer Authentication
- During `PEER_HELLO` handshake, the remote node must prove ownership of its
  advertised node identity (`NodeIdentity`) by signing a challenge with its
  Ed25519 node key.
- `PeerHandshakeManager` must refuse peers that fail the challenge.
- Wire `EncryptedPeerHandshake` → `EncryptedPeerChannel` into the active
  transport path (currently foundations-only).

### 3.2 Peer Discovery Activated
- `DiscoveryService` currently exists as a stub.
- Implement `PEER_STATUS` exchange carrying peer lists; limit list size to
  prevent amplification.
- Integrate with `EclipseGuard` to cap how many peers can share the same /16
  subnet.
- Bootstrap peer list (`BootstrapPeerList`) wired to `NodeDaemon` startup.

### 3.3 Peer Banning and Quarantine
- `PeerAbuseEvidence` and `PeerReputation` exist; wire them to actual banning:
  peers that send invalid messages, flood, or fail authentication are placed in
  a time-limited quarantine before reconnection is allowed.
- `InboundMessageValidator` must be active in the receive path; messages that
  exceed size limits, have bad checksums, or carry unknown types are dropped
  before reaching any handler.

### 3.4 Rate Limiting Hardening
- `PeerRateLimiter` exists; verify it is applied per-message-type, not just
  per-connection.
- Add per-IP and per-subnet rate limits for connection attempts.
- `TRANSACTION_GOSSIP` relay must be rate-limited to prevent amplification
  through `SeenTransactionCache` bypass tricks.

### 3.5 Reconnection Policy
- `PeerReconnectionPolicy` exists; integrate exponential back-off so that a
  disconnected static peer is retried with increasing delays instead of
  immediately.

### 3.6 Eclipse Attack Protection
- `EclipseGuard` foundation exists; activate it: enforce a minimum number of
  outbound connections and a maximum fraction of peers from any single subnet.

**Exit criteria:** a node under peer-flood conditions stays alive, bans
abusive peers, maintains enough honest connections to make progress, and
rejects every message that fails authentication or format validation.

---

## Phase 4 — Validator Staking and Economics

**Goal:** validators lock stake on-chain, their voting weight reflects their
stake, honest validators earn epoch rewards, and dishonest validators are
slashed.

### 4.1 Stake Lifecycle
- `STAKE_DEPOSIT`, `STAKE_TOP_UP`, `STAKE_UNLOCK`, and `STAKE_WITHDRAW` are
  routed through the canonical transaction executor into `StakingRegistry`;
  withdraw finalization is separated from unbonding start.
- State transitions are previewed on copies and the staking domain participates
  in the protocol state root, finalized commit, and replay.

### 4.2 Validator Weight from Stake
- `ValidatorRegistry` must derive each validator's consensus weight from its
  current locked stake (formula: `sqrt(lockedAmount)` to prevent plutocracy).
- Quorum threshold must be recalculated from the new weight distribution every
  epoch.
- Validator with zero unlocked stake must be ineligible to propose or vote.

### 4.3 Epoch Reward Settlement
- `EpochRewardDistributor` and `EpochRewardSettlementService` exist; wire them
  to measurable work: block proposals accepted by quorum, votes cast in rounds,
  uptime metric derived from round participation rate.
- Reward must be bounded by `EpochEmissionPolicy`; no new coins without a
  canonical `GenesisRewardRecord` or authorized emission record.
- `ProtectionRewards` and `RewardDistribution` must be connected to the epoch
  boundary in the block pipeline.

### 4.4 Slash Pipeline
- On confirmed `SlashingEvidence` (double-vote, invalid proposal): deduct a
  configurable fraction of the validator's locked stake.
- `SlashingExecutor` must produce an auditable `PenaltyRecord` that is included
  in the finalized artifact.
- `ValidatorPenaltyApplication` must be idempotent: replaying the same evidence
  must not slash twice.
- Slashed validators must be suspended for the remainder of the epoch.

### 4.5 CLI Commands
- `nodo stake lock --data-dir PATH --validator-key ID --amount RAW_UNITS`
- `nodo stake unlock --data-dir PATH --validator-key ID`
- `nodo stake status --data-dir PATH --validator-key ID`

**Exit criteria:** a validator can lock stake, earn epoch rewards proportional
to participation, and be slashed for double-voting; all flows are auditable via
`chain audit`.

---

## Phase 5 — Governance CLI

**Goal:** operators can submit, vote on, and execute governance proposals
entirely through the CLI, with every decision backed by verifiable evidence.

### 5.1 Proposal Submission
- `GovernanceProposalEnvelope` exists; add CLI command:
  `nodo governance propose --data-dir PATH --title TEXT --body TEXT --type TYPE`
- Proposal must be signed by a registered validator key.
- Proposal must be stored in the persistent governance store and gossipped as
  a new `GOVERNANCE_PROPOSAL` message type.

### 5.2 Voting
- `nodo governance vote --data-dir PATH --proposal-id ID --vote yes|no|abstain --validator-key ID`
- Vote must produce a `GovernanceVoteRecord` signed by the validator.
- `GovernanceTallyService` recomputes the tally after each vote.
- Duplicate votes from the same validator in the same proposal are rejected.

### 5.3 Decision and Execution
- When tally passes threshold and voting period ends: `GovernanceDecisionBuilder`
  produces a `GovernanceDecisionRecord` backed by `GovernanceVoteEvidence`.
- `GovernanceLifecycleStateMachine` transitions the proposal to APPROVED or
  REJECTED.
- For APPROVED treasury proposals: `GovernanceExecutor` wires to
  `GovernanceApprovalBridge` → treasury spend, producing a lifecycle-backed
  execution evidence record.
- `nodo governance execute --data-dir PATH --proposal-id ID` triggers execution
  and persists the result.

### 5.4 Audit
- `nodo governance audit --data-dir PATH` replays all lifecycle records and
  verifies evidence integrity.
- Execution without a valid lifecycle decision must be rejected at the
  finalization layer.

**Exit criteria:** a three-validator testnet can vote to spend treasury funds;
the execution is finalized, persisted, and survives reload audit.

---

## Phase 6 — Storage, Pruning, and Fast Sync

**Goal:** nodes can choose their storage footprint and new nodes can sync
quickly without replaying the entire chain.

### 6.1 Pruning Modes
- `NodePruningConfig` exists; activate three modes:
  - **Archive**: keep all finalized artifacts forever (current default).
  - **Full**: keep all finalized artifacts but prune intermediate state snapshots.
  - **Light**: keep only the last N finalized artifacts; prune older ones after
    verifying the state root chain is intact.
- `StatePruner` must atomically remove pruned files and update the manifest.
- Pruning mode must be recorded in the manifest and enforced on reload.

### 6.2 State Snapshots
- `StateSnapshot` and `StateSnapshotStore` exist; integrate snapshot creation
  at configurable epoch boundaries.
- Snapshot must include: account state, validator registry, coin lot registry,
  governance lifecycle store, stake positions, current supply.
- Snapshot hash must be included in the finalized artifact for the epoch block.

### 6.3 Parallel Block Sync and Fast Sync
- `ParallelBlockSync` exists; wire it to `NodeDaemon` so that a newly started
  node sends `BLOCK_SYNC_REQUEST` to multiple peers and assembles the chain in
  parallel rather than sequentially.
- Snapshot sync: `importSnapshot` currently returns `REJECTED` (explicitly
  unimplemented). Full runtime hydration from a snapshot — account state,
  validators, coin lot registry, staking positions, governance store — must be
  implemented before fast sync is re-enabled. Once implemented, re-introduce the
  snapshot gap routing in `planFromRemoteStatus` and wire to `importSnapshot`.
- `FinalizedArtifactSyncService` must handle the peer-side response path.

### 6.4 Canonical Storage Audit Expansion
- Extend `ChainAuditor` to verify every finalized economic and governance record:
  stake lock lifecycle, epoch reward records, slashing records, governance
  lifecycle records.
- Any missing or tampered record must cause audit failure.

**Exit criteria:** a new node can fast-sync from a snapshot in under one
minute on a local testnet; a pruned node can still pass chain audit.

---

## Phase 7 — JSON-RPC and Light Client

**Goal:** external software (wallets, explorers, tooling) can interact with a
running node over a documented API; light clients can verify block headers
without downloading full artifacts.

### 7.1 JSON-RPC Server
- `JsonRpcServer` exists; expose and document the following methods:
  - `chain_status`: manifest summary, tip height, finalized hash.
  - `tx_submit`: submit a signed transaction; returns tx id or rejection reason.
  - `tx_status`: query if a tx is in mempool, finalized, or unknown.
  - `block_get`: return a finalized block artifact by height or hash.
  - `validator_list`: return active validators with stake and score.
  - `account_state`: return balance, nonce, and coin lots for an address.
  - `governance_proposals`: list active and recent governance proposals.
- All methods must validate inputs; malformed requests must return structured
  error codes, not crashes.
- Rate-limit the RPC endpoint; bind to localhost by default.

### 7.2 Light Client Protocol
- `LightClientMessages` exists; implement the full protocol:
  - Light client requests block headers + `QuorumCertificate` for a range.
  - Full node responds with signed headers and QCs.
  - Light client verifies QC weight against a known validator set root.
  - Light client can request Merkle proofs for specific transactions or accounts.
- `MerkleProof` and `SparseMerkleTree` exist; wire them to the RPC `account_state`
  method to return inclusion proofs.

### 7.3 Event Subscriptions (WebSocket)
- Add WebSocket support to `JsonRpcServer` for streaming:
  - `new_block`: fired on each finalization.
  - `tx_confirmed`: fired when a specific tx is finalized.
  - `validator_slashed`: fired on slash record.

**Exit criteria:** a minimal wallet can submit transactions, query account
state with a Merkle proof, and watch for confirmations without running a full
node.

---

## Phase 8 — Production Key Management

**Goal:** validator keys can be stored securely on production infrastructure
without relying on plaintext files.

### 8.1 Encrypted Key Files
- `KeyEncryptionService` and `KeyEncryptionPolicy` exist; wire them to
  `KeyStore` so that validator BLS12-381 keys are stored encrypted under a
  passphrase-derived key (Argon2id KDF).
- Plaintext key files must be refused on `STAGING_CANDIDATE` and
  `LOCKED_PRODUCTION` networks.
- `keys create` must prompt for a passphrase; `node run` must prompt to unlock.

### 8.2 Key Rotation
- Add `nodo keys rotate --data-dir PATH --old-key ID --new-key ID` which
  produces an on-chain key rotation transaction (signed by both old and new key)
  so that the validator set can accept the new key without a governance proposal.
- The rotation must be reversible only through governance if the old key is
  compromised.

### 8.3 Hardware Security Module Interface
- Define a `HsmSignatureProvider` interface (implements `SignatureProvider`) that
  delegates signing to a PKCS#11-compatible HSM.
- The interface must be testable with a software HSM stub.
- `ProductionKeySafetyGate` must prefer HSM-backed keys over file-backed keys on
  `STAGING_CANDIDATE`.

**Exit criteria:** a validator operator can run a node with all keys encrypted
at rest; signing happens without the raw private key ever entering application
memory in plaintext.

---

## Phase 9 — Testnet Operations

**Goal:** a stable, semi-public testnet-candidate network runs with at least
five independent validators and a documented operator runbook.

### 9.1 Public Testnet Genesis
- Produce a signed `testnet-candidate` genesis config registered in
  `GenesisRegistry`.
- Include at least five validator entries with real BLS12-381 public keys.
- Genesis block must pass `TestnetReadinessChecker` and `ProtocolInvariantChecker`.
- Genesis config must be versioned and immutable after publication.

### 9.2 Validator Onboarding Process
- Document the steps for a new validator to:
  1. Generate and encrypt a BLS12-381 key pair.
  2. Submit a stake lock transaction on testnet.
  3. Register their endpoint in the genesis or via a governance proposal.
  4. Start `nodo node run` and sync to the chain tip.
- `nodo validator register` CLI command that automates steps 1–3.

### 9.3 Network Operations Runbook
- Document: node restart without missing rounds, data-dir migration between
  versions, emergency validator suspension via governance, monitoring via RPC
  `chain_status`.
- Document: expected log messages for healthy operation vs. warning conditions.

### 9.4 CI / CD Pipeline
- Automated build and test on every commit (at minimum: all CTest suites pass,
  `nodo --help` exits 0, localnet round-trip test passes).
- Multi-node integration test in CI: spin up three nodes, finalize ten blocks,
  verify audit passes.
- Fuzz the `NetworkEnvelope` and `FinalizedBlockArtifactCodec` parsers with
  libFuzzer or AFL.

### 9.5 Monitoring Foundations
- `nodo node metrics --data-dir PATH` command that prints JSON:
  current height, last finalized hash, connected peer count, mempool size,
  round number, last block time, validator participation rate in last epoch.
- Prometheus-compatible output optional but desirable.

**Exit criteria:** five validators run continuously for 72 hours without
manual intervention; no block is finalized without quorum; `chain audit` passes
on all five nodes; the runbook is sufficient for a new operator to join.

---

## Phase 10 — Mainnet Readiness

**Goal:** every safety gate is satisfied; an external audit has reviewed the
codebase; the mainnet genesis is signed and published.

### 10.1 External Security Audit
- Full audit scope: cryptographic primitives, consensus BFT safety proofs,
  P2P attack surface, storage integrity, key management, governance execution,
  economic emission policy.
- All critical and high findings must be resolved before any mainnet claim.
- Audit report must be published.

### 10.2 Formal Mainnet Readiness Gate
- `TestnetReadinessChecker` extended with a `MainnetReadinessGate` that verifies:
  - At least 21 independent validators with verified stake.
  - Encrypted key storage enforced for all validators.
  - No `DEVELOPMENT_LOCAL` genesis config present at runtime.
  - External audit complete flag set in a signed attestation record.
  - Governance lifecycle store not empty (at least one governance decision on
    testnet history).
  - Supply audit passes with zero unexplained delta.
- `nodo node run` on a mainnet genesis refuses to start until
  `MainnetReadinessGate` passes.

### 10.3 Mainnet Genesis
- Produce the mainnet genesis config via a multi-party ceremony with verifiable
  randomness for the initial validator set.
- Genesis config registered under `NetworkClass::LOCKED_PRODUCTION` in
  `GenesisRegistry`.
- Initial supply, emission schedule, and treasury seed must be canonical and
  match the published monetary policy.

### 10.4 Production Custody Boundary
- Define and document the custody policy: which keys are operator-held, which
  are protocol-controlled, and how the treasury multi-sig is structured.
- `ProductionKeySafetyGate` must enforce HSM-backed signing for all
  `LOCKED_PRODUCTION` network operations.

**Exit criteria:** mainnet runs for 30 days with at least 21 validators;
`chain audit` passes daily; supply audit matches published emission schedule;
no critical security finding open.

---

## Explicit Non-Goals Until Ready

- No production mainnet claim before Phase 10 is complete.
- No production custody claim before Phase 8 is complete.
- No unaudited treasury execution on any network.
- No governance decision accepted without verifiable vote evidence.
- No monetary expansion without a canonical authorized emission record.
- No mainnet genesis before an external security audit is published.
