# Changelog

Nodo does not yet publish versioned production releases. This changelog starts as a project-level summary for documentation and pre-release development.

## v0.1.3 — 2026-07-10

### Added

- **Protocol domain canonical codecs**: `ProtocolExecutionStateParser` no longer
  reconstructs state via regex/string parsing. Replaced with one codec per
  protocol domain (supply, burns, staking, governance, validators, slashing,
  validator_weights), each with deterministic `encode`/`decode`/`calculateRoot`/
  `validateRoot` built on `CanonicalWriter`/`CanonicalReader`/`CanonicalHash`. A
  new cross-domain integrity check validates the shipped `validator_weights`
  root against the freshly-decoded `validators` payload.
- **Light client cryptographic verification**: `LightClientProtocolVerifier`
  gained `verifyFinalizedHeader`/`verifyFinalizedHeaderChain`, which verify the
  `QuorumCertificate` (per-vote BLS signatures, validator weight vs. threshold,
  `validator_set_root`) and the finalized record for a header or header range,
  reusing per-height validator-set history so a validator-set transition
  mid-range is checked against the correct set on each side. Wired into the
  RPC-serving `headerRangeJson`/`checkpointJson` paths; `checkpointJson`
  previously did not verify anything at all.
- **Typed `ProposalJustification`** (`NONE` / `UNLOCK_QUORUM_CERTIFICATE`)
  replaces the ad hoc raw quorum-certificate string the BFT lock/unlock safety
  rule used to carry, with `permitsUnlock()` as an independently testable
  enforcement point.
- **Validator key rotation transaction type** (`VALIDATOR_KEY_ROTATE`): payload
  schema, validator-registry rotation (preserves owner, stake, and status),
  staking-registry address rotation, and transaction-builder support.
- **Unified testnet-candidate key policy**: a password-encrypted local key
  (`TESTNET_SAFE`) is now sufficient and consistently enforced across every
  signing CLI command (`tx submit`, `governance propose/vote/execute`,
  `validator exit/unjail`, `stake lock` family, `node run`, `keys create`).

### Fixed

- **Real fast-sync data-loss bug**: nodes fast-syncing from a snapshot
  previously lost all governance state (proposals, votes) and most staking
  detail (positions, owner splits, lifecycle records), because the old
  regex-based parser stubbed the governance domain outright and coarsely
  re-derived staking from validator stake instead of parsing it. Closed by the
  canonical domain codecs above; covered by an extended `FastSyncImportTests`
  scenario that fails before the fix and passes after.
- **Testnet-candidate was unconditionally blocked regardless of key quality**:
  `ProtocolCryptoContext::testnet()` was permanently invalid by construction
  (a hardcoded rejection reason plus a `productionSafe()` check that could
  never pass), which blocked `tx submit`, `governance propose/vote/execute`,
  `validator exit/unjail`, and `stake lock` on testnet-candidate outright.
  `node run` was the sole command unaffected, only because it bypassed the
  same crypto-context gate entirely — an accidental inconsistency, not a
  deliberate exemption; it now goes through the same gate as every other
  command.
- **`keys create` blanket-blocked every official network**, not just mainnet,
  leaving its own already-implemented testnet-appropriate password-prompt path
  unreachable — there was previously no CLI path to create a testnet-candidate
  key at all.
- **`ProductionKeySafetyGate` accepted under-encrypted keys**: it special-cased
  only `PLAINTEXT`, so a `DEV_ENCRYPTED` key — below the `TESTNET_SAFE` bar the
  policy itself requires — silently passed on official networks. It now
  reuses `KeyEncryptionPolicy::isAcceptable()`.
- **Windows/MinGW CI build failure**: two CLI test files called POSIX-only
  `setenv`/`unsetenv` directly; replaced with a portable helper
  (`_putenv_s` on `_WIN32`, `setenv`/`unsetenv` elsewhere).

### Changed

- Modernized block state snapshot synchronization; removed the ad hoc scratch
  scripts (`parse_gen.py`, `parse_test.py`, `run_build.sh`) used during
  fast-sync development.

### Documentation

- Reorganized documentation structure: archived superseded files and
  standardized path/naming conventions (`docs/ROADMAP.md` →
  `docs/roadmap.md`, `docs/serialization/CANONICAL_SERIALIZATION.md` →
  `canonical-serialization.md`, consolidated transaction docs, removed the
  stale `docs/testnet-local.md`).
- Updated `docs/operations/networks-and-data-directory.md`,
  `docs/security/key-management.md`, and `docs/status.md` to describe the
  enforced (not aspirational) testnet-candidate key policy.

### Tests

- New/expanded coverage: `ProtocolDomainCodecTests`, extended
  `FastSyncImportTests`, `LightClientProtocolTests` (10 cases, including
  forged-signature and stale-registry-across-a-transition negatives), new
  `LightClientServiceTests`, fully rewritten `ConsensusLockUnlockTests`
  (a locked validator rejects an unjustified or invalid-QC vote, accepts a
  valid-QC unlock, and never finalizes two conflicting blocks at one height),
  `ValidatorKeyRotationTests`, extended `ProtocolCryptoContextTests` and
  `ProductionKeySafetyGateTests`, and a new `CommandLineNetworkKeyPolicyTests`.

## v0.1.2 — 2026-07-07

### Removed

- **Protocol uniformization pass — dead and parallel code paths deleted.** A
  full reference audit of the include graph (the GLOB-based build compiles
  every `src/*.cpp`, so unused modules never fail the build) found ~38 modules
  with no production references, each superseded by the live official path.
  Removed, with their dedicated tests:
  - node: `SlashingExecutor` (→ `CanonicalSlashingTransition`),
    `GovernanceTallyService` (→ `economics` governance tally/audit path),
    `TestnetReadiness` legacy monolith (→ `TestnetReadinessChecker`,
    `BlockAnnounceHandler`, `ChainSyncMessages`, `HealthCheckService`),
    `BlockSyncHandler` (→ `PersistentBlockStateSync`), `StateReplayAuditor`
    (→ `ChainAuditor`), `ValidatorPenaltyMessages`
    (→ `SlashingEvidenceMessages`), `ValidatorSecurityPosture`,
    `TreasuryReportDeriver`, `LocalNetworkStateInspector`,
    `ProtocolCompletenessGate`, `ProtocolSafetyGate`;
  - core: `CoinLotRegistryRebuilder`, `ValidatorProposalAdmission`,
    `BloomFilter`, `LightClientProof` (documented-incorrect sibling-position
    assumption; never wired);
  - storage (legacy pre-`RuntimeStateLoader` pipeline): `BlockchainLoader`,
    `BlockchainStorageReader`, `BlockStorageIndex`, `ChainManifest`,
    `BlockFileStore`, `StorageRecovery`, `StorageMigration`,
    `ValidatorPenaltyStore`;
  - serialization: `ChainManifestCodec`, `BlockStorageIndexCodec`,
    `ConsensusCanonicalCodec`;
  - consensus: `ChainReorgGuard` (→ `ForkChoice`), `ValidatorAccountability`;
  - crypto: `SignatureProviderRegistry` (→ `ProtocolCryptoContext`),
    `NodeIdentity` (identity proof lives in `PeerHandshakeManager`);
  - economics: `StakeSlashApplication`, `ValidatorPenaltyLedgerBuilder`
    (→ `consensus::ValidatorPenaltyApplication` via
    `CanonicalSlashingTransition`);
  - mempool: `FeeMarket` (→ `node::FeeEconomics` + admission policy);
  - staking: `StakingManager` (→ `node::StakingRegistry`);
  - p2p: `LightClientMessages` (→ `node/LightClientProtocol`),
    `BootstrapPeerList` (→ `node run --peer` + `registerBootstrapPeer`),
    `EncryptedPeerHandshake` (→ `PeerHandshakeManager` +
    `PeerSessionKeyAgreement`), `PeerAbuseEvidence` (→ peer-store quarantine
    in `TcpTestnetNodeRuntime`).

  Roadmap-planned but not-yet-wired modules were deliberately kept
  (`ParallelBlockSync`, `StateSnapshotStore`, `FinalizedArtifactSyncService`,
  `GovernanceLifecycleStore`, `DefenseModeGuard`/`DefenseModeTransitionApplier`,
  `core/ChainStateRebuilder` as test scaffolding).

### Fixed

- **Documentation drift**: `docs/ROADMAP.md` claimed `BootstrapPeerList` and
  `PeerAbuseEvidence` were "wired ✅" — they never were; the entries now
  describe the actual live mechanisms. README's QC persistence flow no longer
  lists the removed `BlockSyncHandler` entry point. Eight technical docs no
  longer reference deleted classes.

## v0.1.1 — 2026-07-06

### Fixed

- **Data race on `NodeRuntime` between the RPC thread and the daemon tick/consensus
  thread**: `NodeOrchestrator` now owns a `std::mutex` shared with `NodeRpcServer`
  (acquired for the whole `dispatch()` call, covering every request handler) and
  with `NodeDaemon`'s `processTransactionGossip`, `processLocalMempoolSubmissions`,
  `processFinalizedArtifacts`, and `maybeProposeBlock` (acquired around each,
  alongside `NodeOrchestrator::tick()`'s own internal lock). Without it, a
  concurrent block commit could be observed mid-mutation by an RPC request —
  reproduced as a corrupted block index during canonical protocol replay under
  sustained concurrent load.
- **Duplicate peer-maintenance registration** in the P2P peer-connection loop.
- **Transaction relay budget used the wrong time unit**, under- or over-counting
  the per-second relay allowance.
- **Periodic P2P peer maintenance was not being invoked**; re-enabled.

### Added

- **Governance-executed treasury spends**: a governance proposal can carry a
  `treasurySpend` payload that, once approved, is automatically queued for
  execution at block finalization and later moved only by an explicit,
  permissionless `GOVERNANCE_EXECUTE` transaction. The finalized artifact for the
  execution block embeds real treasury execution evidence, independently
  re-verifiable by `chain audit` and `governance audit` after a fresh reload.
- **`GovernanceGossipE2ETests`**: a real multi-node (3 validators, real TCP)
  end-to-end test proving a governance proposal submitted at one node becomes
  votable on the other two through gossip alone, that each validator's owner can
  vote through a different node's RPC, and that the resulting tally and executed
  decision are byte-identical across all three nodes.
- Deterministic validator owner key seeds for reproducible multi-node test setups.
- Deterministic validator weight calculation, integrated into the protocol state
  root.
- **`--json` flag** across CLI commands for structured, script-friendly output.
- End-to-end test coverage for the validator stake lifecycle and slashing
  mechanics.

### Changed

- Replaced hardcoded network string parameters with a proper `NetworkParameters`
  configuration object.
- Reorganized includes and reimplemented `ProtocolTransactionDomainExecutor` for
  clearer structure.
- Removed ad hoc debugging patch scripts in favor of targeted diagnostic output
  in transaction propagation tests.
- Increased the CTest timeout and filler-block retry budget for the real
  multi-node governance gossip test.

## v0.1.0 — 2026-07-05

First tagged snapshot of the Nodo protocol: chain state transition and
validation, BFT-style consensus with slashing for equivocation and double
voting, a full validator staking lifecycle, on-chain governance, an encrypted
P2P networking stack (authenticated peer sessions, EclipseGuard subnet limits,
peer reputation with time-bounded bans, exponential-backoff reconnection,
authenticated peer exchange), a mempool with fee-based eviction, and CLI/RPC
tooling. The entries below cover the hardening pass immediately before tagging.

### Core — State Transition

- **CoinLot validation wired into `StateTransitionPreview`**: when a
  `StateTransitionPreviewContext` is configured with `enableCoinLotPreview`, every
  TRANSFER transaction in a candidate block is validated by
  `CoinLotTransactionValidator::applyTransfer` against a working copy of the
  `CoinLotRegistry`. Transfers that exceed available lots, spend already-spent
  lots, or violate ownership rules are rejected with `INVALID_TRANSACTION` before
  the block reaches the vote stage. Double-spend within the same block is caught
  because the working registry is mutated in order.

- **CoinLot registry digest in `stateRoot`**: after all transactions are applied,
  the final working registry is serialized, hashed, and inserted into the state
  root computation under the `coin_lots` domain via
  `StateRootCalculator::calculateProtocolStateRoot`. Divergent registry state
  between proposer and validator produces a mismatched root and causes
  `BlockValidationMode::ProtocolCommitment` rejection.

- **`StateTransitionPreviewContext` API hardened**: removed dead
  `m_coinLotPreviewEnabled` / `m_supplyAuditPreviewEnabled` flags. Replaced with
  `std::optional<CoinLotRegistry> m_coinLotRegistry`. `coinLotPreviewEnabled()`
  returns `m_coinLotRegistry.has_value()`. Added `enableCoinLotPreview(registry)`
  and `coinLotRegistry()` (throws if preview not enabled).

### Consensus

- **`ConsensusEventLoop` authorization guard**: the proposal processing loop in
  `drainProposals` now skips any proposal when
  `validationContext.protocolAuthorizationEnabled()` is false (chain identifier
  not configured or crypto context invalid). Votes are never cast when signatures
  cannot be verified.

### Node — Block Sync

- **`importSnapshot` now returns `REJECTED`**: the previous implementation was a
  silent no-op (`(void)` casts on all parameters) that let callers believe a
  snapshot had been applied when nothing happened. It now returns
  `PersistentSyncApplyStatus::REJECTED` with a clear diagnostic: snapshot sync
  requires full runtime hydration and is not yet implemented.

- **`planFromRemoteStatus` no longer routes to `REQUEST_SNAPSHOT`**: the snapshot
  gap threshold branch (`heightGap >= SNAPSHOT_GAP_THRESHOLD → REQUEST_SNAPSHOT`)
  has been removed. All height gaps now use `REQUEST_BLOCKS` unconditionally,
  avoiding calls to the broken snapshot path and preventing checkpoint corruption.

### Tests

- **4 new `StateTransitionPreviewTests`**: empty registry rejects transfer,
  sufficient lots accept transfer, coin lots change combined state root, and
  double-spend within a block is rejected with `processedTransactionCount == 1`.
- **`BlockStateTransitionValidatorTests`**: renamed and updated test to clarify
  that rejection for an unauthorized context goes through stateRoot mismatch;
  authorization guard lives in the consensus path.
- **`PersistentBlockStateSyncPlannerTests`**: updated far-ahead case to assert
  `requestBlocks()` instead of `requestSnapshot()`; fixed `maxBlocks` assertion
  to use `ProtocolLimits::MAX_PERSISTENT_SYNC_BLOCK_BATCH`.

### Documentation

- Refreshed public README and documentation structure.
- Documented current pre-mainnet status, Proof-of-Protection principles, build/test commands, architecture, security posture, treasury evidence, and governance lifecycle audit.
- Updated `STATE_TRANSITION.md`, `PERSISTENT_BLOCK_STATE_SYNC.md`,
  `CONSENSUS_RULES.md`, `economics/COIN_LOT_REGISTRY.md`, and
  `economics/COIN_LOT_TRANSACTION_INTEGRATION.md` to reflect all of the above.
