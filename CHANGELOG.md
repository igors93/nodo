# Changelog

Nodo does not yet publish versioned production releases. This changelog starts as a project-level summary for documentation and pre-release development.

## Unreleased

### Security

- **Validator sets are frozen for each epoch in development protocol
  `nodo/0.4`.** Finalized epoch boundaries project the next voting set with
  two-boundary activation delay and a 3333-basis-point changed-weight budget.
  Historical QC verification, replay, reload and artifact import use the
  selected set. Oversized changes are staged, and history stores snapshots
  only when the set changes. This is incompatible with `nodo/0.3` state;
  [ADR 0003](docs/spec/adr-0003-epoch-validator-sets.md) records the rule and
  remaining production prerequisites.
- **Quorum is strictly greater than two thirds of historical voting weight.**
  Profiles accept only the canonical 2/3 fraction; vote pools and certificates
  require `floor(2W/3)+1` weight, and certificate validation rejects a forged
  threshold. The incompatible development protocol is `nodo/0.3`; there is no
  in-place migration from `nodo/0.2`. [ADR 0002](docs/spec/adr-0002-fault-model-and-quorum.md)
  defines the Byzantine fault bound and partial-synchrony assumptions.
- **Validator voting power is linear in recorded active stake.** Splitting a
  stake across keys no longer increases aggregate voting weight. Registry
  transitions reject weight overflow. The change was introduced in the
  `nodo/0.2` development rules; older state has no in-place migration.
  [ADR 0001](docs/spec/adr-0001-validator-weight.md) records
  the design and remaining v1 stake-backing requirements.
- **testnet-candidate genesis no longer embeds derivable keys.** Its
  bootstrap validator keys, owner keys, and funded account were derived from
  fixed seeds in `GenesisRegistry.cpp`, so anyone could recompute every
  private key. The seeds are removed and `testnet-candidate` has no built-in
  genesis: `GenesisRegistry::get("testnet-candidate")` now reports that an
  operator genesis document is required.
- **Official-network keys come from the OS CSPRNG.** `nodo keys create` on
  `testnet-candidate` used `genesisConfigId#keyType#keyId` (or, for the
  default key ids, the localnet seeds) as the key seed. It now uses
  `KeyStore::createRandomKey`. `localnet` and `localnet-soak` keep
  deterministic keys for reproducible development.
- **JSON-RPC requests are parsed by nlohmann/json** (v3.12.0, pinned by
  SHA-256) instead of a hand-rolled substring search. The old parser took the
  first `"method"`, `"id"`, or parameter name found anywhere in the body, so a
  key nested inside `params` could choose which handler ran or which value a
  handler read. Requests are now rejected on malformed JSON, trailing data,
  invalid UTF-8, duplicate keys at any depth, nesting deeper than 32 levels,
  unknown top-level members, or mistyped `jsonrpc`/`method`/`params`/`id`.
- **The RPC HTTP server runs on Asio instead of raw sockets.** The old server
  spawned one thread per connection and kept every finished thread until
  `stop()`, so threads accumulated with each request; it had no connection
  cap; and its 2-second timeout applied per `recv`, so a client sending one
  byte every 1.9 s held a thread for hours. Connections are now asynchronous
  sessions on a fixed pool of 4 threads, capped at 128 (32 WebSocket
  subscribers), and a whole request must arrive within 10 s.
- On Windows the RPC listener no longer sets `SO_REUSEADDR`, which there
  allows another process to bind the same port.

### Fixed

- Six copies of a hand-written `jsonString` escaped only `"`, `\`, `\n`,
  `\r` and `\t`, so any other control byte or invalid UTF-8 in chain data
  produced invalid JSON in health, metrics, event, light-client and RPC
  responses. All of them now use `utils::jsonString` (nlohmann/json).
- Malformed, oversized, or chunked HTTP requests were closed without a
  response; they now get `400`, `413`, `431`, or `501`. Unread request bytes
  are drained before closing, so the client sees the status instead of a
  connection reset.
- WebSocket client frames split across reads, or several frames in one read,
  were mis-decoded; `WebSocketFrame` now reports the bytes it consumed. An
  unmasked client frame closes the stream, as RFC 6455 requires.
- `stop()` waited up to 3 s for each open WebSocket subscriber; it now
  returns as soon as the worker threads exit.
- `--rpc-listen` now accepts bracketed IPv6 literals (`[::1]:8545`) and host
  names; the server previously accepted only IPv4 literals.
- Asio warned on every Windows translation unit about `_WIN32_WINNT`; it is
  now defined as Windows 10.

### Added

- **Operator genesis document** (`NODO_GENESIS_DOCUMENT_V1`,
  `config::GenesisDocumentCodec`): carries the genesis timestamp, memo,
  bootstrap validator BLS public keys with owner addresses, and funded
  accounts. Network parameters always come from the code profile; the
  document's chain id and protocol version must match it. Decoding rejects
  unknown fields, non-canonical hex, bad address checksums, duplicates, and
  too few validators.
- **`nodo genesis create`** builds the document from public keys only and
  never overwrites an existing file; **`nodo genesis inspect`** prints its
  genesis id for operators to compare.
- **`nodo init --genesis-file PATH`** initializes a `testnet-candidate` data
  directory from the document. Later commands read the copy pinned in
  `{dataDir}/genesis.nodo`, whose id the manifest must match.
- `nodo keys create --network testnet-candidate` works before `init`, so
  operators can generate keys before the genesis exists, and prints each
  key's public key.

### Changed

- RPC responses use `HTTP/1.1` status lines (still `Connection: close`);
  `NodeRpcServer::Limits` makes the transport limits configurable, and port
  `0` binds an ephemeral port reported by `port()`.
- JSON-RPC responses echo the request `id` with its JSON type (`7` stays a
  number) and answer parse errors with `"id":null`; previously every id was
  returned as a string. `\uXXXX` escapes in parameters are now decoded, and
  error messages always serialize to valid JSON.
- `init` writes `genesis.nodo` as a genesis document for every network
  (previously a write-only debug serialization).
- Existing `testnet-candidate` data directories were initialized from the
  revoked built-in genesis and must be re-initialized from a ceremony genesis.

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

### Finalized slashing evidence sync audit

Finalized block sync no longer depends on a peer having seen the original slashing-evidence gossip. When a synchronized finalized block carries `SLASHING_EVIDENCE` records, the import path replays the block, verifies that each evidence id produced exactly one `ValidatorPenaltyDecision`, and audits that `ValidatorRegistry` and `StakingRegistry` mirror the finalized jail/tombstone/slash effects before publishing the new runtime state. Evidence that was still pending locally is removed from the pending evidence store once its penalty is finalized by block sync.

### Mandatory P2P hardening boundary

The live TCP testnet path now treats P2P security controls as mandatory protocol admission gates, not optional helpers. Non-handshake traffic must arrive through an authenticated encrypted peer session, every envelope is validated against network id, chain id, protocol version, TTL, clock skew, duplicate message id and payload hash, rate limits are enforced per peer and per message type, repeated abuse quarantines and disconnects the peer, and peer admission is checked by `EclipseGuard` before registration. Local loopback tests may still instantiate `GossipMesh` without the hardened config, but `TcpTestnetNodeRuntime` always enables the hardened path.

### Discovery and reconnection policy

Bootstrap peers, UDP discovery results and disconnected authenticated peers now enter one deterministic reconnection policy before any TCP attempt is made. The daemon no longer connects discovered/static peers through an immediate shortcut: candidates are tracked, seeded into discovery, retried with exponential backoff, capped per tick, and suppressed when peer quarantine state is active. This keeps peer discovery useful without allowing tight reconnect loops or bypassing the hardened P2P admission gate.

### Authenticated peer exchange

Peer exchange is now a canonical authenticated P2P message. Nodes broadcast capped `PEER_EXCHANGE` payloads only through authenticated encrypted sessions, parse them through the strict peer-exchange codec, screen each candidate with `EclipseGuard`, persist accepted reconnect candidates separately from trusted peer metadata, and route every learned peer through `PeerReconnectionPolicy` instead of opening direct sockets.

### Connection slot policy

The TCP testnet transport now treats connection capacity as a protocol admission policy. Pending handshakes remain capped by total/IP/subnet limits and token buckets, while authenticated connections are capped by total, inbound, outbound, per-IP and per-/24 subnet slots. When a total or directional slot is full, the oldest replaceable connection is evicted deterministically; when an IP or subnet is saturated, new peers are rejected instead of weakening diversity. This keeps discovery and peer exchange useful without allowing one address block to occupy the node.

### P2P reputation and temporary bans

Peer abuse handling is now persistent and time-bounded. Repeated invalid or rate-limited traffic lowers the peer score, creates audit evidence, applies a temporary ban with `bannedUntil` and canonical reason, disconnects the peer, suppresses reconnect attempts while the ban is active, and lifts the ban deterministically after expiry. Peer penalty state is stored in `peers.conf` together with score, quarantine flag and invalid-message count.

### Light client and event stream

Nodo now exposes light-client primitives through JSON-RPC (`light_getCheckpoint`,
`light_getHeaders`, `light_getAccountProof`, `light_getTransactionProof`) and a
WebSocket-compatible event stream at `GET /events`. This is intended for wallets,
explorers and monitoring tools that need finalized headers, account proofs,
transaction proofs and live finalization/submission notifications without running
a full archival node.

### Production key-management foundation

Nodo now includes a `VALIDATOR_KEY_ROTATE` protocol transaction and an external-signer-ready validator signing boundary. This is a pre-mainnet foundation for key rotation and future HSM-backed signing.
