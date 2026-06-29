# Changelog

Nodo does not yet publish versioned production releases. This changelog starts as a project-level summary for documentation and pre-release development.

## Unreleased

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
