# Roadmap

This roadmap lists every known problem that stands between the current code and a strong, production-grade Layer 1, ordered from the base of the system upwards. Each problem was found by reading the code; each one names the file or function where it lives.

> **Last full code review:** 2026-10-06, commit `baeb369`.
> **Current phase:** Phase 0 — Engineering base.

## How this roadmap works

- **Phases are strictly sequential.** Phase N+1 starts only when every item of phase N is resolved and the phase's exit gate is met. Lower phases are the layers that higher phases are built on, so fixing them later would force rework above them.
- **Resolved** means the fix is merged with a regression test and the documentation is updated. A design item is resolved by a written, reviewed decision in the specification. An item is never resolved by deleting a test or weakening a check.
- **New problems** get the next free ID in the phase whose layer they affect. If that phase is already closed, the current phase's gate cannot close until the new item is fixed.
- **Severity:**
  - *Critical:* breaks consensus safety or liveness, supply integrity, or makes the node unusable at scale.
  - *High:* exploitable by a peer, user, or validator, or blocks a later phase.
  - *Medium:* a correctness or robustness gap.
  - *Low:* hygiene.
- **Item IDs** (`4.1`, `5.3`, …) are stable. Reference them in issues, commits, and pull requests.

## Phase overview

| Phase | Name | Why it sits here | Status |
| --- | --- | --- | --- |
| 0 | Engineering base | Build, CI, docs and code structure must be trustworthy before protocol code is rewritten. | Current |
| 1 | Protocol specification and design decisions | Several current design choices are unsafe; they must be decided on paper before bytes, state, consensus, or economics are re-implemented. | Blocked by 0 |
| 2 | Encoding, hashing and cryptography | Every higher layer hashes, signs, and stores these bytes. | Blocked by 1 |
| 3 | State, execution and storage engine | Consensus validates and commits through this layer; today it costs O(chain length) per block. | Blocked by 2 |
| 4 | Consensus safety and liveness | The BFT protocol runs on top of state and storage and must be proven safe and live. | Blocked by 3 |
| 5 | Networking, sync and light clients | Sync and light clients must be anchored to the finality evidence produced in Phase 4. | Blocked by 4 |
| 6 | Economics and validator accountability | Incentives depend on correct consensus, time, and stake rules. | Blocked by 5 |
| 7 | Governance, treasury and protocol upgrades | Governance changes parameters and rules that Phases 1–6 define. | Blocked by 6 |
| 8 | Keys, custody, RPC and node operations | Operators need a stable protocol before custody and runbooks can be finalized. | Blocked by 7 |
| 9 | Public testnet and external audits | Only a complete system can be audited and run by independent operators. | Blocked by 8 |
| 10 | Mainnet readiness | Launch only after everything above is proven. | Blocked by 9 |

## Most critical findings

These ten findings explain the phase order. Each one is detailed in its phase.

1. **Stake splitting multiplied voting power in `nodo/0.1`** (1.2, 4.8). The former `floor(sqrt(stake))` rule let one party gain weight by creating keys. Since `nodo/0.2`, weight is linear in recorded raw stake. V1 additionally requires every unit of voting weight to be backed by a unique locked stake unit; genesis backing and complete v1 migration remain open.
2. **A height can stall forever** (4.1). A locked validator can only unlock with a PRECOMMIT quorum certificate for the new block, and proposers never re-propose the locked block.
3. **Nil votes are broadcast before they are persisted** (4.2). A crash in between lets the validator double-sign after restart.
4. **Evidence and unbonding need aligned windows** (1.5, 4.9, 6.3). `nodo/0.5` enforces bounded evidence age and longer dual-clock unbonding, while censorship-resistant inclusion remains a production gate.
5. **Every block commit costs O(chain length)** (3.1–3.3). The whole `NodeRuntime`, including every block, is copied on each commit, and some validation paths replay the chain from genesis.
6. **Height is used as a vector index** (3.4). After a snapshot reset, RPC, gossip admission, fork choice and reward code read the wrong block.
7. **One slow peer stalls the node** (5.1). Blocking reads inside the single daemon loop let a peer that trickles bytes block consensus.
8. **Fast sync is not anchored to finality** (5.3). A snapshot is checked only against a manifest supplied by the same peer, and the import path is wired only in tests.
9. **Development monetary policy runs on every network** (6.1, 6.2). The "annual" inflation caps are measured in block counts calibrated for 60 s blocks.
10. **The block "header" is the whole block** (1.6, 5.6). The block hash covers every record, and the header has no chain id or validator-set commitment, so light clients cannot work from headers.

---

## Phase 0 — Engineering base

**Goal:** a repository whose build, CI, documentation and module structure can be trusted, so every later protocol change is reviewed and tested on solid ground.

- [x] **0.1 · High · No license.**
  The repository has no `LICENSE` file, and the README says redistribution rights are undefined. Outside operators, auditors and contributors cannot legally rely on the code.
  *Fix:* choose and add a license and contributor terms.

- [x] **0.2 · Medium · Documentation contradicts the code.**
  - The README links to `docs/PERSISTENT_BLOCK_STATE_SYNC.md`, which does not exist.
  - The README storage layout (`manifest`, `schema`, `sync/checkpoint.conf`) contradicts [storage and reload](architecture/storage-and-reload.md) (`manifest.nodo`, `storage_schema.nodo`).
  - The README says `ConsensusEventLoop` runs on a background thread, but `NodeDaemon` ticks it synchronously.
  - The README has changelog-style sections appended after "License".
  - [Testing](getting-started/testing.md) and [Build](getting-started/build.md) use `build` as the CTest directory, while the scripts use `build/cmake`.
  - The [module map](architecture/module-map.md) assigns stake lifecycle to a `staking` module, but `src/staking` only contains `SecurityWeight`, and `StakingRegistry` lives in `src/node`.
  - [Local testnet](operations/local-testnet.md) presents isolated nodes as a testnet (see 0.14).

  *Fix:* do one pass to make the docs describe actual behavior, and add a Markdown link checker to CI.

- [x] **0.3 · Low · Build artifacts are committed.**
  `diagnostics/python/dist/*.whl`, `*.tar.gz` and `diagnostics/python/src/nodo_diag.egg-info/` are tracked.
  *Fix:* remove them and ignore them.

- [x] **0.4 · Low · Version sources disagree.**
  CMake declares `project(Nodo VERSION 0.1.0)`, the changelog is at `v0.1.3`, and the network profiles use protocol version `nodo/0.1`.
  *Fix:* use a single software-version source, and keep the protocol version separate and explicit.

- [x] **0.5 · Medium · Supply chain is partly unpinned.**
  Asio is fetched by git tag without a hash in [`cmake/NodoDependencies.cmake`](../cmake/NodoDependencies.cmake); only nlohmann/json is pinned by SHA-256. blst is discovered from `$HOME` with no version check (CI uses v0.3.11). The first configure needs network access.
  *Fix:* pin every dependency by hash, verify the blst version at configure time, and support offline or vendored builds.

- [x] **0.6 · Medium · No structured logging.**
  `src/` outside `src/app` contains 30 writes to `std::cout`/`std::cerr`, including `[DEBUG]` prints in `NodeDaemon`, `NodeOrchestrator`, `NodeRpcServer`, `Mempool` and `MempoolBlockProducer`. `ConsensusEventLoop` still has `// debug loop` leftovers.
  *Fix:* add a leveled logger with component tags, and remove stdout writes from library code.

- [x] **0.7 · Medium · Protocol helpers and concepts are duplicated.**
  There are 24 local copies of `isSafeScalar` with diverging limits (for example, the one in `consensus/SlashingEvidence.cpp` takes a `maxSize`), and 16 local `hashString` helpers. Some concepts exist twice:
  - `node::MonetaryPolicy` and `economics::MonetaryPolicy`, whose inflation constant "must stay in sync";
  - `node::SlashingEvidenceRecord` and `consensus::SlashingEvidenceRecord`.

  *Fix:* one shared validation and hashing utility, and one type per concept.
  *Implemented:* scalar and hash implementations are centralized; the two
  differently shaped monetary and slashing records now have distinct names.

- [x] **0.8 · Medium · Layering violations and oversized files.**
  `src/consensus` and `include/consensus` include `node/` headers, which reverses the dependency direction in the module map. They include `NodeRuntime`, `RuntimeBlockPipeline`, `EpochRewardSettlementService`, slashing gossip and more, 23 includes in total. Some files are very large: `CommandLineInterface.cpp` (4.1k lines), `FinalizedBlockArtifactCodec.cpp` (3.7k), `RuntimeBlockPipeline.cpp` (2.1k), `NodeRpcServer.cpp` (2.0k).
  *Fix:* define the narrow interfaces consensus needs from the runtime, and split the files by responsibility. Phases 3 and 4 rewrite these paths.
  *Implemented:* runtime orchestration moved into `node/consensus`, leaving
  `consensus` independent of `node` headers. CLI parsing and stake commands,
  artifact state and codec, RPC transport and handlers, and pipeline execution
  and result validation are split into separate translation units. The large
  codec and CLI methods will be redesigned in later phases.

- [x] **0.9 · Low · Non-English comments.**
  Eight source and header files have Portuguese comments (for example `include/utils/Amount.hpp` and `include/crypto/PrivateKey.hpp`), which breaks the English-only rule in `CONTRIBUTING.md`.

- [x] **0.10 · Low · Swap-prone configuration constructor.**
  `NetworkParameters` is built from 24 positional arguments (see `NetworkParameters::developmentLocal`). Two swapped integers compile silently.
  *Fix:* use a named-field struct or a builder.

- [x] **0.11 · Low · Non-portable arithmetic.**
  `unsigned __int128` is used in consensus math (`ConsensusWeight`, `QuorumCertificateBuilder::requiredVotingWeight`), which builds on GCC and Clang only.
  *Fix:* use a portable checked 128-bit helper, or document the supported compilers.

- [x] **0.12 · High · CI does not catch whole classes of bugs.**
  - There is no ThreadSanitizer job, although the RPC server (4 Asio workers), the discovery thread and the transport share state.
  - There is no fuzzing, no static analysis (clang-tidy, cppcheck, CodeQL), and no coverage report.
  - Warnings are not errors.
  - The Release build compiles but runs no tests.
  - The real-TCP multi-node tests are disabled on Windows.

  *Fix:* add TSan, static analysis, coverage, `-Werror`, and Release test runs, plus fuzzing infrastructure (the fuzz targets come in 2.11).
  *Implemented:* TSan, cppcheck, coverage, Release CTest, `-Werror`, and a
  libFuzzer smoke target are configured. Windows now runs a real authenticated
  two-daemon TCP handshake test; process-isolated scenarios remain POSIX-only.
  The new CI jobs still require their first green run.

- [x] **0.13 · Low · No shared test framework.**
  All 327 test files define their own `main` and ad-hoc assertion helpers, and each builds a separate executable.
  *Fix:* adopt one test framework with shared fixtures and fewer binaries.
  *Implemented:* CMake groups tests into module runners with stable CTest
  names; source-level `main` functions become named runner entries. Shared
  integration fixtures and the common `require` helper live in `tests/common`.
  Release tests keep assertions enabled.

- [x] **0.14 · Medium · The "local testnet" is not a network.**
  `scripts/testnet_local_multi_node.sh` runs `block produce` independently on each node. The result is N isolated chains with no consensus between them.
  *Fix:* build a real multi-validator devnet (`node run` with `--peer`, for example with docker compose) and run it in CI.
  *Implemented:* `node_FourValidatorDevnetTests` launches four processes over authenticated TCP and checks common finality and persisted chain audit. Linux and macOS CTest jobs include it. Its first CI run is pending; the Windows fork-based harness does not run it.

**Exit gate:**
- CI is green on Linux (GCC, Clang, ASan/UBSan, TSan), macOS and Windows, with `-Werror` and static analysis.
- The docs link check passes, and the docs describe actual behavior.
- A license is present, dependencies are pinned, and logging is in place.
- A devnet with at least 4 validators runs real networked consensus in CI.

**Gate status:** awaiting the first green multi-platform CI run, including
Linux's four-validator devnet and sanitizer/static-analysis jobs. The local
Windows build, documentation check, targeted protocol tests and authenticated
real-TCP handshake pass.

---

## Phase 1 — Protocol specification and design decisions

**Goal:** write the rules of the L1 down so later phases implement a specification instead of evolving code. Several current design choices are unsafe and must be decided here, because they change data formats and the consensus security model.

The [v1 design contract](spec/protocol-v1.md) records target rules for several
items below. Those items stay open until their rationale is captured in
architecture decision records, the required formal checks and external review
are complete, and the affected implementation work is scheduled. The v1
contract is not wire-compatible with the current development runtime.

- [x] **1.1 · Critical · There is no protocol specification.**
  The rules exist only as prose docs plus code.
  *Fix:* write specification v1 covering:
  - the state machine (accounts, coin lots, staking, validators, governance, treasury);
  - transaction and block validity rules, with every rejection reason enumerated;
  - the consensus protocol and finality;
  - canonical encoding;
  - network messages.
  *Design contract:* [protocol v1](spec/protocol-v1.md) defines the target
  state machine, all 13 transaction types, ordered rejection codes, block and
  finality rules, canonical binary encoding and authenticated network messages.
  [Primitive byte vectors](spec/vectors-v1.md) anchor the encoding rules.
  This is an incompatible target protocol, not a claim that `nodo/0.5` already
  implements it. Full signed-object vectors, architecture decision records,
  formal checking and external BFT review remain in the Phase 1 exit gate;
  runtime migration remains in Phases 2–8.

- [x] **1.2 · Critical · The validator weight model is Sybil-amplifiable.**
  `nodo/0.1` used `floor(sqrt(stake))`, so splitting stake across `k` validators
  multiplied voting power by approximately `√k`. The minimum was 0.01 NODO.
  *Decision and implementation:* [ADR 0001](spec/adr-0001-validator-weight.md)
  proves the split invariant and rejects square-root weight, per-validator
  weight caps and naive top-K active-set caps. [Protocol v1](spec/protocol-v1.md)
  requires linear weight backed one-to-one by locked stake. The development
  runtime now uses linear weight, the existing 0.01 NODO minimum, checked
  aggregate weight and version separation. Regression tests cover split stake,
  historical snapshots, minimum stake and overflow. The remaining stake-lot
  backing (3.16), genesis migration, epoch timing and active-set resource
  design remain open; this checkbox does not mark them complete.

- [x] **1.3 · Critical · The fault model and quorum are unspecified.**
  The development runtime used `ceil(2W/3)`, which allowed exactly two thirds
  when `W` was divisible by three, contrary to the v1 contract. Parameter
  validation already rejected fractions below two thirds, but accepted higher
  fractions with different liveness assumptions; QC verification accepted
  noncanonical required-weight metadata.
  *Decision and implementation:* [ADR 0002](spec/adr-0002-fault-model-and-quorum.md)
  and [protocol v1](spec/protocol-v1.md) define Byzantine weight `B < W/3`,
  safety before and after GST, and conditional liveness under partial
  synchrony. `nodo/0.3` accepts only the canonical 2/3 parameter pair and
  requires exactly `floor(2W/3)+1` signed weight from the height's validator
  set. QC metadata, construction and vote-pool progress use the same checked
  rule. Boundary and intersection tests cover the decision. System-wide
  safety and liveness still require Phase 4 locking, persistence and formal
  verification; this checkbox records the Phase 1 rule and arithmetic.

- [x] **1.4 · Critical · The validator set changes every block.**
  Previously, replay, commit, reload and import copied the mutable economic
  registry into the next height, so stake and status changes could alter
  consensus membership in the middle of an epoch.
  *Decision and implementation:* [ADR 0003](spec/adr-0003-epoch-validator-sets.md)
  fixes consensus sets for all 43200 heights of an epoch. The final block
  deterministically projects the next set with two-boundary activation delay
  and at most 3333 basis points of changed voting weight; excess changes are
  staged. Proposal and QC paths use the historical set. History stores full
  registries only when the selected set changes. Boundary, staged-churn and
  Sybil-split tests cover the policy. The one-to-one locked-stake backing and
  genesis stake backing (3.16), light-client transition proofs and
  dynamic-set formal verification remain separate production gates.

- [x] **1.5 · Critical · Unbonding is shorter than any evidence window.**
  [ADR 0004](spec/adr-0004-accountability-windows.md) fixes the development
  evidence limit at 21 validator epochs and unbonding at 28 epochs **and** at
  least 28 days plus the current future-block margin. Execution checks both
  deadlines and retains stake backing every still-slashable historical vote;
  key rotations preserve liability against the successor account. Canonical
  and gossip admission reject expired evidence. New nodes require an
  out-of-band checkpoint no older than 14 days under the v1 design decision.
  BFT time (1.7), guaranteed evidence inclusion (4.9), genesis backing
  (3.16), and automated checkpoint enforcement (Phase 5) remain production
  gates.

- [ ] **1.6 · High · The block header is not a header.**
  The block hash is SHA-256 of a text "header" that embeds every record (`Block::headerPayload` in [`Block.cpp`](../src/core/Block.cpp)), so hashing or proving a header requires the whole block. The header also lacks several commitments: chain id, protocol version, proposer, validator-set hash, next-validator-set hash, previous-commit (QC) hash, consensus-parameters hash, and evidence hash.
  *Fix:* design a compact header with separate record, receipt and state roots.

- [ ] **1.7 · High · There is no time model.**
  Block time is the proposer's wall clock, checked only for `> previous` and `≤ now + 300 s` (`BlockStateTransitionValidator`). Rounds are timed with local whole-second clocks.
  *Decide:* BFT time (for example, the weighted median of precommit timestamps) and clock-skew bounds. All time-based economics must use it.

- [ ] **1.8 · High · Epoch and block cadence are inconsistent.**
  - `NetworkParameters::epochDurationSeconds` (60 s or 300 s) is never used.
  - Validator epochs are hard-coded at 43,200 blocks (`NODO_VALIDATOR_EPOCH_BLOCKS`).
  - Issuance windows are hard-coded at 525,600 blocks (`NODO_CONTROLLED_ISSUANCE_EPOCH_BLOCKS`).
  - `EpochEmissionPolicy` assumes 365 epochs per year.
  - The target block time (60 s localnet, 30 s testnet) is not enforced: blocks are produced as fast as a round completes, limited only by whole-second timestamps.

  *Decide:* one cadence model, either height-based epochs with an enforced block time or time-based epochs on BFT time, and move every constant into the genesis parameters.

- [ ] **1.9 · High · Two serialization formats, one of them ad-hoc text.**
  Consensus objects are hashed and signed as text built with `std::ostringstream` (`Name{a=…;b=…}`), while a binary `CanonicalWriter` is used for other objects.
  *Fix:* specify one binary canonical encoding, with versioning and test vectors for every consensus object.

- [ ] **1.10 · High · There is no resource or fee model.**
  The fee is an absolute amount per transaction, and the mempool orders by absolute fee (`sortedEntriesByPriority` in [`Mempool.cpp`](../src/mempool/Mempool.cpp)). A 200 KiB transaction pays the same as a 200-byte one, and nothing meters bytes, signature checks, or state writes.
  *Decide:* fees per resource unit, block resource limits, and a base-fee mechanism if any.

- [ ] **1.11 · High · There is no protocol upgrade mechanism.**
  Rules are not versioned by activation height; the protocol version is just the string `nodo/0.5`.
  *Specify:* how rule changes activate (height-gated rule sets, version in the header, client signalling) and how historical rules stay replayable.

- [ ] **1.12 · Medium · Programmability is undecided.**
  Nodo has 13 fixed transaction types and no VM.
  *Decide:* whether launch requires smart contracts (and which VM, for example WASM), or whether Nodo stays a fixed-function L1 with a later upgrade path. The answer changes the state model, fees, and storage.

- [ ] **1.13 · Medium · The proposer selection rule is weak.**
  `ProposerSchedule::selectProposer` draws a hash lottery per `(height, round)`. It gives no fairness bound over short windows, has a modulo bias, and is predictable arbitrarily far ahead, which makes targeted DoS on upcoming proposers easy.
  *Specify:* weighted round-robin (proposer priority) or a VRF-based rule.

- [ ] **1.14 · Medium · Liveness faults have no accountability.**
  Penalty reasons are only `DOUBLE_SIGN`, `INVALID_PROPOSAL`, `INVALID_SIGNATURE` and `MANUAL_REVIEW` (`ValidatorPenaltyRecord.hpp`). There is no downtime or missed-vote jailing, and `MANUAL_REVIEW` contradicts "no penalty without evidence".
  *Specify:* downtime rules, and remove or strictly bound manual penalties.

- [ ] **1.15 · Medium · No formal model.**
  *Fix:* write a TLA+ (or Quint/Apalache) model of the voting, locking and round rules decided here, and model-check agreement and validity.

**Exit gate:**
- Specification v1 is published under `docs/spec/` with test vectors.
- Decisions 1.2–1.14 are recorded as architecture decision records.
- The formal model passes.
- At least one external reviewer with BFT experience has reviewed the specification.

---

## Phase 2 — Encoding, hashing and cryptography

**Goal:** implement the byte-level base defined in Phase 1. Every higher layer hashes, signs, and stores these bytes.

- [ ] **2.1 · High · Implement the canonical binary encoding** (1.9) for transactions, headers, votes, quorum certificates, receipts, evidence and snapshots. Remove text serialization from every hashing and signing path, and keep text only for display and diagnostics.

- [ ] **2.2 · High · Hashes are strings.**
  Hashes travel as 64-character hex `std::string`. `MerkleTree::hashNode` hashes the concatenation of two hex strings, which doubles the input. The magic strings `"GENESIS"` and `"SNAPSHOT"` stand in for hashes.
  *Fix:* add a fixed 32-byte hash type and remove the string sentinels.

- [ ] **2.3 · High · Hash failures are silent.**
  `nodo_hash_bytes` ([`hash.c`](../src/crypto/hash.c)) returns `void` and writes an empty string when OpenSSL fails. Callers can compare two empty "hashes" as equal.
  *Fix:* make hashing failure fatal or propagated.

- [ ] **2.4 · High · The Merkle tree is mutable.**
  `MerkleTree::buildRoot` sorts leaves, so their order is not committed. It also duplicates the last node on odd levels: `[a,b,c]` and `[a,b,c,c]` produce the same root (the CVE-2012-2459 class). The tree size is not committed either.
  *Fix:* an RFC 6962-style tree with ordered leaves, no duplication, and the size in proofs.

- [ ] **2.5 · Medium · The sparse Merkle tree is rebuilt for every root.**
  `StateRootCalculator` builds a new 256-level pointer tree (`std::vector<bool>` keys) from all accounts and validators on every call.
  *Fix:* use an incremental authenticated structure (compact SMT or JMT). Phase 3 persists it in the storage engine.

- [ ] **2.6 · High · Signed objects are malleable.**
  `crypto::Signature` carries `createdAt`, which is serialized with the signature but not signed: both providers sign `bindMessage(message, domain)` only. A relayer can change the bytes of a transaction, vote or QC without invalidating any signature. Byte comparisons of finalized records, as in `PersistentBlockStateSync`, then become unreliable.
  *Fix:* remove `createdAt` from signatures, or bind it into the signed message.

- [ ] **2.7 · High · No BLS proof of possession.**
  `VALIDATOR_REGISTER` and `VALIDATOR_KEY_ROTATE` carry a BLS public key authorized only by the owner's Ed25519 signature (`TransactionBuilder::buildSignedValidatorRegistration`), so anyone can register someone else's key. `Bls12381SignatureProvider::aggregateSignatures` adds public keys without proof of possession, which allows a rogue-key attack if aggregation is ever used.
  *Fix:* require a proof of possession under a dedicated DST at registration and at rotation.

- [ ] **2.8 · Medium · Quorum certificates are O(n).**
  A QC carries every individual vote and is verified signature by signature.
  *Fix:* once 2.7 is in place, switch to an aggregate signature plus a signer bitmap.

- [ ] **2.9 · Medium · Secrets are not wiped.**
  Private keys are hex `std::string` values (`PrivateKey`) that get copied freely and are never wiped. `OPENSSL_cleanse` is used nowhere, and the `memset` calls in `KeyEncryptionService` can be optimized away.
  *Fix:* a non-copyable secure buffer type that cleanses on destruction.

- [ ] **2.10 · Medium · The key-file KDF is weak.**
  Key files use PBKDF2-HMAC-SHA256 with 200,000 iterations (`KeyEncryptionService::PBKDF2_ITERS`).
  *Fix:* Argon2id (or scrypt) with versioned parameters and a migration path.

- [ ] **2.11 · High · No fuzzing or cross-implementation vectors.**
  *Fix:* add fuzz targets for every decoder that reads untrusted input: transactions, blocks, votes, QCs, network envelopes, encrypted frames, genesis documents, snapshots, JSON-RPC. Publish encoding and signing test vectors.

**Exit gate:**
- All consensus objects use the specified encoding and pass the published vectors.
- No hex or text remains in hashing paths.
- Proof of possession is enforced.
- The fuzzers run in CI with a defined CPU budget and no open crashes.

---

## Phase 3 — State, execution and storage engine

**Goal:** a state machine and storage layer whose cost per block does not grow with chain length, and that survives a crash at any instruction.

- [ ] **3.1 · Critical · The whole runtime is copied on every commit.**
  `RuntimeBlockPipeline::commitCertifiedBlock` executes `NodeRuntime stagedRuntime = runtime;` ([`RuntimeBlockPipeline.cpp`](../src/node/RuntimeBlockPipeline.cpp)). That copies every block, the validator-set history, every registry, and the mempool on each block, so the cost is O(chain length) per block with double the memory.
  *Fix:* a write batch or overlay on top of the state store.

- [ ] **3.2 · Critical · The whole chain lives in memory.**
  `Blockchain` still holds every `Block` in a `std::vector`, so memory grows
  with chain length. `ValidatorSetHistory` now stores full registries only at
  actual set changes, but the block and state caches remain unbounded.
  *Fix:* keep blocks on disk behind a bounded cache and use transactional
  state overlays instead of copying the whole runtime.

- [ ] **3.3 · Critical · Hot paths replay from genesis.**
  `ProtocolStateTransition::replayNextBlock` calls `replayToTip`, which re-executes the whole chain; `GovernanceArtifactValidator` uses it. `RuntimeAccountStateBuilder` and `RuntimeStateVerifier` also replay to the tip.
  *Fix:* validate from the persisted tip state, and keep full replay for audit only.

- [ ] **3.4 · Critical · Height is used as a vector index.**
  After `Blockchain::resetFromSnapshot`, the vector starts with a placeholder block at height H. The following code still reads `blocks()[height]` and returns the wrong block or nothing:
  - `Blockchain::blockByHeight`;
  - the finalized-block callback in `NodeOrchestrator`;
  - `FinalizedArtifactGossipAdmission`;
  - `NodeRpcServer` (block by height);
  - `ForkChoice`;
  - `ProtocolInvariantChecker`;
  - `SyncRecoveryPolicy`;
  - `EpochParticipation`;
  - `EpochRewardSettlementService`.

  In addition, `replayToTip` compares `blocks().front()` with the genesis block.
  *Fix:* index blocks by height in the store.

- [ ] **3.5 · High · The snapshot placeholder block skips validation.**
  `Block::createSnapshotDummy` sets `previousHash = "SNAPSHOT"`, and `Block::isValid` skips the hash check for it.
  *Fix:* represent the snapshot base as a trusted header obtained by QC-anchored state sync (5.3), not as a fake block.

- [ ] **3.6 · Critical · No storage engine and no atomic multi-file commit.**
  Each finalized block writes the block artifact, the QC file, the manifest and runtime files as separate files. A crash between them leaves a mixed state that reload can only detect, not repair.
  *Fix:* an embedded key-value store (RocksDB or LMDB) with one atomic write batch per block: block, state diff, QC, indexes and manifest.

- [ ] **3.7 · High · No durability on Windows.**
  `AtomicFile::writeTextFile` only fsyncs on `__unix__`/`__APPLE__`. Windows gets no `FlushFileBuffers` and no directory sync. This is resolved by 3.6 if the engine handles durability; otherwise fix it directly.

- [ ] **3.8 · High · No indexes.**
  Transaction-by-id and block-by-hash RPC lookups scan every block in `NodeRpcServer` while holding the global runtime mutex.
  *Fix:* add `txId → (height, index)` and `hash → height` indexes.

- [ ] **3.9 · High · RPC can starve consensus.**
  RPC handlers, the daemon and consensus share one `std::mutex` over `NodeRuntime`.
  *Fix:* give RPC an immutable committed snapshot view.

- [ ] **3.10 · High · Validators do not enforce block limits.**
  `NetworkParameters::maxTransactionsPerBlock` (500 on testnet-candidate) is applied only by the local producer. Validators accept up to `ProtocolLimits::MAX_BLOCK_RECORDS` (10,000 records, 1 MiB) from any proposer.
  *Fix:* enforce every limit from 1.10 in block validation.

- [ ] **3.11 · Medium · Amounts are signed.**
  `utils::Amount` is a signed 64-bit integer, so negative amounts can be represented anywhere.
  *Fix:* unsigned checked arithmetic (or 128-bit), and reject negative values at decode.

- [ ] **3.12 · Medium · Implement resource metering** (1.10) in execution and the mempool: fee per resource unit, per-sender limits, and eviction by fee rate.

- [ ] **3.13 · Medium · Pruning is undefined on a real store.**
  *Fix:* implement archive and pruned modes with the proofs each mode must keep (open item in [sync, pruning and snapshots](development/sync-pruning-snapshots.md)).

- [ ] **3.14 · Medium · A certified block can be rejected locally, with no defined outcome.**
  `RuntimeBlockPipeline::applyCertifiedBlock` re-validates a block that already has a quorum certificate and rejects it on any local failure, for example the monetary gate. What happens next is undefined.
  *Fix:* treat this as a consensus failure: halt with a clear operator alarm, and never diverge silently.

- [ ] **3.15 · Medium · No storage migrations.**
  The docs require explicit, versioned, tested migrations, but only schema-version checks exist.

- [ ] **3.16 · Critical · Bootstrap voting stake is synthesized rather than proven by locked coin lots.**
  `GenesisBuilder::build` floors each `bootstrapWeight` to `MIN_VALIDATOR_STAKE_RAW_UNITS` before registering its voting weight. The resulting registry entry is not a proof that distinct genesis coin lots of that amount were locked to that validator. Linear weights alone cannot establish a stake-based Byzantine bound without that backing.
  *Fix:* encode exact bootstrap stake amounts and unique stake-lot commitments in genesis; reject missing, duplicated, underfunded or mismatched allocations; derive each initial validator's weight only from the locked lots. Apply the same one-to-one invariant to later registrations, stake changes, state replay and snapshots, with adversarial conservation tests.

**Exit gate:**
- A benchmark with 1M blocks and 1M accounts shows flat commit time and memory as the chain grows.
- Crash-injection tests (kill at every write point) always reload to a consistent state.
- An RPC load test does not delay block production.

---

## Phase 4 — Consensus safety and liveness

**Goal:** a BFT consensus that is safe while Byzantine weight is below 1/3 and live after GST, shown by model checking and by adversarial simulation.

- [ ] **4.1 · Critical · Locked validators can never unlock, so a height can stall forever.**
  `ProposalJustification::permitsUnlock` accepts only a PRECOMMIT quorum certificate for the new block, which means a block that is already decided. `NodeDaemon::maybeProposeBlock` always builds a fresh block with `ProposalJustification::none()` and never re-proposes the locked block. If at least 1/3 of the weight locks on a block that does not finalize in that round, those validators refuse every later proposal and the height never finalizes.
  *Fix:* Tendermint-style `validValue`/`validRound`. Proposers re-propose the valid block with its proof-of-lock round, and validators unlock on a later-round proof-of-lock (2/3+ PREVOTEs).

- [ ] **4.2 · Critical · Votes are exposed before they are persisted.**
  In `ConsensusEventLoop::tick` ([`ConsensusEventLoop.cpp`](../src/node/consensus/ConsensusEventLoop.cpp)), nil PREVOTE and PRECOMMIT votes are broadcast by `submitAndBroadcastSignedVote` before `saveRecoveryState()` runs, and its return value is ignored (in the no-candidate branch too). A crash in between lets the node sign a conflicting vote for the same round after restart, which is a slashable double-sign.
  *Fix:* persist before signing and broadcasting, for every vote, and fail closed if persisting fails.

- [ ] **4.3 · High · No double-sign guard in the signer.**
  Nothing inside the signing boundary checks the last signed `(height, round, step)`. Safety depends on flags in the event loop.
  *Fix:* a monotonic guard inside the signer, also used by the remote signer (8.1).

- [ ] **4.4 · High · Nil votes never reach peers when no proposal arrives.**
  The no-candidate branch calls `m_runtime.submitConsensusVote(nilVote)` without broadcasting it, so no validator ever observes a nil quorum.
  *Fix:* broadcast nil votes, and implement the rules "2/3+ PREVOTEs for nil → PRECOMMIT nil" and "2/3+ PRECOMMITs for anything → start the precommit timeout".

- [ ] **4.5 · High · No round synchronization.**
  Proposals are accepted only for the current or next round (`processBlockProposals`), and rounds advance only on each node's own timer since `roundStartedAt`. There is no "skip to round r after f+1 messages of round r" rule, so validators whose rounds drift apart may never vote in the same round.
  *Fix:* implement round skipping and catch-up.

- [ ] **4.6 · High · Timeouts are fixed and whole-second.**
  Timeouts do not grow per round, which partial-synchrony liveness requires. Elapsed time is computed as `(now - start) * 1000` from whole-second timestamps, and `NodeRuntime::advanceConsensusRoundIfTimedOut` truncates the millisecond sum to whole seconds.
  *Fix:* a monotonic millisecond clock and timeouts that increase per round.

- [x] **4.7 · High · The next proposer is computed from the wrong validator set.**
  Timeout rounds, block production, commit, reload and artifact import now
  select the proposer from `validatorSetHistory().setAt(height)`, the same
  frozen set used by voting and proposal validation. Epoch-boundary tests
  exercise old and new sets separately.

- [ ] **4.8 · Critical · Complete the Phase 1 consensus decisions:** consume
  one-to-one stake backing (3.16), prove the epoch transition and bounded
  churn rules of 1.4 against the dynamic-set fault model, implement the v1
  evidence-driven jail exception, BFT time (1.7) and proposer rule (1.13).

- [ ] **4.9 · High · Complete the evidence lifecycle.**
  `nodo/0.5` enforces a 21-epoch maximum age on evidence admission and
  execution. *Remaining:* guarantee inclusion despite censorship, retain and
  prune evidence consistently with the window, and make recent evidence
  available to weak-subjectivity checkpoint clients.

- [ ] **4.10 · Medium · Nil is a magic string.**
  A nil vote is encoded as decision `REJECT` with `blockHash = "nil"` and `previousHash = "nil"`.
  *Fix:* model nil as a typed vote value.

- [ ] **4.11 · High · No deterministic consensus simulation.**
  *Fix:* a harness that runs N validators in one process over a controllable network (delay, drop, reorder, partition) with Byzantine behaviors (equivocation, withholding, invalid proposals), run in CI with randomized seeds.

- [ ] **4.12 · Medium · Consensus is polled, not event-driven.**
  `NodeDaemon::runBlocking` calls `tick()` and sleeps, so consensus reacts only at tick granularity.
  *Fix:* an event loop driven by message arrival and timers.

**Exit gate:**
- The formal model (1.15) matches the implemented rules.
- At least 10,000 randomized simulation seeds, including Byzantine weight just under 1/3, show no safety violation and always finalize after GST.
- A 72-hour devnet chaos run (crashes, partitions, clock skew) shows no double-sign and no stalled height.

---

## Phase 5 — Networking, sync and light clients

**Goal:** a peer-to-peer layer that a hostile peer cannot stall, and sync paths that accept only data anchored to finality evidence.

- [ ] **5.1 · Critical · One slow peer stalls the whole node.**
  `TcpTransport` reads frames with `readAll`/`writeAll` helpers ([`TcpTransport.cpp`](../src/p2p/TcpTransport.cpp)). They wait up to 250 ms per partial read and keep looping while bytes trickle in, all inside the single daemon tick. A peer that sends one byte every ~200 ms can block consensus for as long as a 5 MiB frame takes. `FD_SET` is also used without an `FD_SETSIZE` guard, which is undefined behavior above 1,024 descriptors on Linux; Windows is limited to 64 sockets per `select`.
  *Fix:* rewrite P2P I/O on Asio (as the RPC server already is), with per-peer buffers and deadlines.

- [ ] **5.2 · High · The transport cryptography is home-made.**
  The handshake is custom (`PeerSessionKeyAgreement`, `EncryptedPeerChannel`): X25519, HKDF and AES-256-GCM with hash-derived nonces and no rekeying. Frames are hex-encoded, which doubles bandwidth.
  *Fix:* adopt an analyzed handshake (Noise XX/IK, or TLS 1.3 with peer-key pinning) and binary frames.

- [ ] **5.3 · Critical · Fast sync is not anchored to finality.**
  `FastSyncSnapshotVerifier` checks only the genesis id, the chain id, and that the snapshot matches a manifest supplied by the source peer. Nothing checks a quorum certificate for the snapshot height or a validator-set chain from genesis. `PersistentBlockStateSyncApplier::importSnapshot` is called only from tests.
  *Fix:* state sync that verifies a light-client chain of QCs and validator-set transitions (or a weak-subjectivity checkpoint) up to the snapshot header, then downloads chunked state with proofs against the state root.

- [ ] **5.4 · High · Sync fetches one block per round trip.**
  `ProtocolLimits::MAX_PERSISTENT_SYNC_BLOCK_BATCH` is 1.
  *Fix:* ranged, pipelined, multi-peer block sync. `ParallelBlockSync` exists; wire it in and bound it.

- [ ] **5.5 · High · Gossip floods.**
  Every message goes to every peer, large objects have no announce/request path, and per-peer limits count messages rather than bytes or CPU.
  *Fix:* inventory/pull for blocks and large transactions, topic meshes, and rate limits based on bytes and cost.

- [ ] **5.6 · High · Light clients have no real headers.**
  `light_getHeaders` returns `headerPayload`, which embeds all records.
  *Fix:* header-only sync on the new header (1.6), with validator-set transition proofs and bounded proof sizes.

- [ ] **5.7 · Medium · Validator node protection is undefined.**
  *Specify:* sentry and private-peer topologies, and how peer identity keys relate to validator keys and rotate (8.2).

- [ ] **5.8 · Medium · No adversarial network test suite.**
  *Fix:* automate slowloris, oversized frames, per-type floods, eclipse attempts, reconnect storms and malformed handshakes against the devnet in CI.

**Exit gate:**
- The adversarial suite passes.
- A 100-node emulated network with latency and loss keeps finalizing.
- A fresh node syncs a 1M-block chain both by state sync and by full replay within the published target times.
- The transport handshake has been reviewed.

---

## Phase 6 — Economics and validator accountability

**Goal:** monetary and incentive rules whose parameters live in genesis, whose time base is correct, and which survive adversarial analysis. This is the Proof-of-Protection layer.

- [ ] **6.1 · Critical · The development monetary policy runs on every network.**
  Block validation (`RuntimeMonetaryValidation`), `ChainAuditor` and the CLI all build `economics::MonetaryPolicy::localnetDefault(…)` whatever the network. `EpochRewardSettlementService` uses `EpochEmissionPolicy::developmentDefaultPolicy()` for every network.
  *Fix:* put the monetary and emission policy in the genesis document and the state commitment, and refuse to start without it.

- [ ] **6.2 · Critical · "Annual" caps are not annual.**
  The inflation cap window is 525,600 blocks, which is one year only at 60 s blocks. Reward emission spreads the yearly target over 365 epochs of 43,200 blocks each, which is one day only at 2 s blocks. The real block time follows consensus speed (1.8), so effective yearly inflation swings with block speed: about 0.13 % at 60 s blocks, the 4 % target at 2 s, and 8 % at 1 s. At 1 s blocks, the 4 % cap window resets about every six days.
  *Fix:* re-derive emission and caps from the cadence model and test them across block times.

- [ ] **6.3 · High · Complete stake windows and liveness accountability:** the `nodo/0.5` unbonding and evidence windows are enforced (1.5); BFT time (1.7), parameterized v1 windows, stake-lot backing, and downtime rules (1.14) remain.

- [ ] **6.4 · High · "Measurable protection work" is undefined.**
  *Fix:* define work metrics that a validator cannot generate by itself (votes included in QCs, finalized proposals, accepted evidence), make the validator score a deterministic function of on-chain records, and cap rewards per epoch.

- [ ] **6.5 · High · No economic simulation or abuse analysis.**
  *Fix:* agent-based simulation of rewards, fees, slashing, stake splitting, cartel and censorship incentives, and treasury drain; publish the rationale for every parameter.

- [ ] **6.6 · Medium · Two sources of truth for inflation.**
  `economics::MonetaryPolicy::MAX_ANNUAL_INFLATION_BASIS_POINTS` and `node::NODO_MAX_ANNUAL_INFLATION_BASIS_POINTS` must be kept equal by hand. Keep one definition. The code cleanup is 0.7; this item covers the economic semantics.

- [ ] **6.7 · Medium · Coin-lot state growth is unbounded.**
  Every balance carries lot lineage.
  *Fix:* measure proof and state growth, and define consolidation or compaction rules.

- [ ] **6.8 · Medium · Testnet parameters are not locked.**
  *Fix:* lock supply, epoch length, inflation cap, reward cap, fee policy, minimum stake, unbonding, penalty sizes and treasury limits (the list in [economics overview](economics/economics-overview.md)).

**Exit gate:**
- Economics specification v1 is published with the rationale for every parameter.
- A simulation report is published.
- The supply invariant (genesis + minted − burned − slashed) is checked on every devnet block.
- An external economic review is complete.

---

## Phase 7 — Governance, treasury and protocol upgrades

**Goal:** on-chain decisions and rule changes that cannot bypass validation and can be executed safely on a live network.

- [ ] **7.1 · Critical · Implement the upgrade mechanism** (1.11): height-activated rule sets, version signalling, replay with historical rules, and a coordinated upgrade procedure.

- [ ] **7.2 · High · Governance rules are not final.**
  *Fix:* settle the voting-power source (a stake snapshot at proposal start), quorum, thresholds, voting period, timelock, cancellation and veto, and the emergency path. These are the open items in the [governance docs](governance/governance-overview.md).

- [ ] **7.3 · High · Governable parameters are unbounded.**
  *Fix:* give every governable parameter bounds and rate-of-change limits. Consensus-critical parameters change only through an upgrade (7.1).

- [ ] **7.4 · Medium · The governance propagation path is unclear.**
  The README says proposal gossip across peers is not wired, while `GovernanceGossipE2ETests` exists.
  *Fix:* verify and document one path: proposals and votes travel only as transactions in finalized blocks.

- [ ] **7.5 · Medium · Treasury safeguards are incomplete.**
  *Fix:* document and enforce the treasury emergency process and per-epoch limits, and require multi-party execution authority (8.1).

- [ ] **7.6 · Medium · Audits depend on local stores.**
  *Fix:* governance and treasury audits must be reproducible from finalized blocks alone.

**Exit gate:**
- A rule change activated at a fixed height has been rehearsed on devnet with mixed-version nodes.
- Governance and treasury flows, including their rejection paths, have run end-to-end on a test network.

---

## Phase 8 — Keys, custody, RPC and node operations

**Goal:** validators run without hot keys on disk, and operators can observe, upgrade and recover nodes.

- [ ] **8.1 · High · There is no remote signer.**
  `crypto::OutofProcessSigner` exists but is not wired into any command.
  *Fix:* a remote-signer protocol with an authenticated channel, the double-sign guard from 4.3 and an audit log, with HSM/KMS backends.

- [ ] **8.2 · High · Rotation and revocation workflows are incomplete.**
  *Fix:* define and document key rotation and revocation for validator, peer-identity and owner keys, with proof of possession (2.7).

- [ ] **8.3 · High · The RPC surface is flat.**
  There is no authentication and no method classes. REST and JSON-RPC duplicate protocol routes, and rate limits count requests per IP rather than per-method cost.
  *Fix:* split the public read API, transaction submission, and the admin/ops API; authenticate the admin API; add cost-based limits and TLS/reverse-proxy guidance; retire the duplicated REST protocol routes.

- [ ] **8.4 · Medium · Observability is incomplete.**
  *Fix:* structured logs (0.6); metrics for consensus steps (round, step, votes, timeouts), P2P, storage and mempool; alerting rules; block-lifecycle tracing.

- [ ] **8.5 · Medium · No release engineering.**
  *Fix:* reproducible builds, signed release artifacts, an SBOM, container images, a compatibility matrix, and storage migrations on upgrade (3.15).

- [ ] **8.6 · Medium · No runbooks.**
  *Fix:* runbooks for installation, the key ceremony, backup and restore, incident response, upgrades, recovery from a consensus halt (3.14), and hardware and network requirements.

- [ ] **8.7 · Low · No client SDK.**
  *Fix:* a reference library that builds and signs transactions with the canonical encoding (2.1) and passes the published test vectors.

**Exit gate:**
- Test-network validators run only with remote signers.
- Runbooks have been rehearsed in drills (key loss, node loss, halt recovery).
- The release pipeline produces signed, reproducible builds.

---

## Phase 9 — Public testnet and external audits

**Goal:** independent operators run the network under real conditions, and independent experts review the code.

- [ ] **9.1** Run a public testnet genesis ceremony with independent validators spread across operators, jurisdictions and hosting providers.
- [ ] **9.2** Run a soak test of at least 90 days with scheduled chaos (validator crashes, partitions, upgrades) against published SLOs for finality latency and missed blocks.
- [ ] **9.3** Run an incentivized adversarial period and a bug bounty.
- [ ] **9.4** Complete external audits of consensus, cryptography, P2P, state and storage, and economics. Every finding is fixed, or accepted with a written rationale, and the reports are published.
- [ ] **9.5** Run continuous fuzzing (for example, OSS-Fuzz) on all decoders and state transitions.
- [ ] **9.6** Measure throughput, block time, finality, sync time and hardware requirements on the public testnet.

**Exit gate:**
- No Critical or High finding is open.
- The testnet met its SLOs for 90 consecutive days.
- At least one upgrade has been executed on the testnet.

---

## Phase 10 — Mainnet readiness

**Goal:** launch only when every earlier gate is met.

- [ ] **10.1** Define the mainnet network profile. Today `NetworkProfileRegistry` rejects `mainnet` and no profile exists; build it from the Phase 6 and Phase 7 parameters.
- [ ] **10.2** Run the mainnet genesis ceremony and publish the genesis document.
- [ ] **10.3** Prepare the launch checklist, go/no-go criteria, halt and rollback procedures, and disaster recovery.
- [ ] **10.4** Complete a legal and compliance review of the token and the treasury.
- [ ] **10.5** Give final approval only when all of the following are true:
  - external audits are complete;
  - independent validators run the network;
  - economic parameters are stable;
  - production custody is in place;
  - the governance and treasury emergency processes are documented;
  - state replay is deterministic;
  - release and rollback procedures exist;
  - disaster recovery has been rehearsed.

### After mainnet

These tracks start only if Phase 1 decided they are not needed at launch:

- programmability (VM, metering, contract storage), if 1.12 deferred it;
- performance (parallel execution, larger validator sets with aggregated signatures).

---

## Foundations already in the code

These were verified in the 2026-10-06 review. They are a starting point, not finished work: several items above reopen them.

- Localnet pipeline: init, key creation, transaction submission, local block production (development profiles only), reload, chain audit.
- Finalized block artifacts, manifest and storage-schema validation, and QC persistence under `sync/qc/`.
- Two-phase PREVOTE/PRECOMMIT voting with BLS12-381 votes, QCs built from PRECOMMIT votes only, and a consensus recovery store.
- Ed25519 user signatures, BLS validator signatures, signing domains, and a mandatory chain id in transactions.
- TCP transport with authenticated encrypted sessions, rate limits, temporary bans, an eclipse guard, peer exchange, and a reconnection policy.
- Staking registry, validator lifecycle transactions, and slashing evidence for double votes and proposer equivocation with deterministic penalties.
- Governance proposals, votes, decisions and execution with lifecycle audit; treasury policy and execution evidence.
- JSON-RPC parsed by nlohmann/json on an Asio server; health and metrics; light-client RPC methods; an event stream.
- Canonical domain codecs for snapshots, the testnet-candidate genesis ceremony, and a per-network key-safety gate.
- 327 test executables, Python diagnostic scenarios, and CI on Linux, macOS and Windows with ASan/UBSan.

## Non-goals until the mainnet gate

Nodo must not claim or enable any of the following before Phase 10 is complete:

- production mainnet;
- real treasury value;
- custody of user funds;
- permissionless public validator onboarding without safety gates;
- final monetary policy claims;
- final legal or compliance claims;
- irreversible governance execution on a live-value network.

## Maintaining this roadmap

- Check an item only when its fix is merged with a regression test and updated documentation, and append the commit or pull request reference.
- Add a newly found problem with the next free ID in the phase of the layer it affects, with its evidence (file and function) and a fix.
- Re-run a full code review at every phase gate and update **Last full code review** at the top.
