# Changelog

Nodo does not yet publish versioned production releases. This changelog starts as a project-level summary for documentation and pre-release development.

## Unreleased

Nothing yet.

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
