<p align="center">
  <img src="assets/nodo-readme-hero.svg" alt="Nodo - Proof of Protection Blockchain" />
</p>

<p align="center">
  Security-first blockchain infrastructure for verifiable protection, auditable economics, controlled treasury execution, governance evidence, and rebuildable state.
</p>

<p align="center">
  <a href="https://github.com/igors93/nodo/actions/workflows/ci.yml"><img src="https://github.com/igors93/nodo/actions/workflows/ci.yml/badge.svg?branch=main" alt="CI" /></a>
  <img src="https://img.shields.io/badge/C%2B%2B-20-blue" alt="C++20" />
  <img src="https://img.shields.io/badge/build-CMake-informational" alt="CMake" />
  <img src="https://img.shields.io/badge/status-in%20development-orange" alt="In development" />
  <img src="https://img.shields.io/badge/network-pre--mainnet-lightgrey" alt="Pre-mainnet" />
  <img src="https://img.shields.io/badge/security-first-success" alt="Security-first" />
</p>

<p align="center">
  <a href="#overview">Overview</a> |
  <a href="#features">Features</a> |
  <a href="#quick-start">Quick Start</a> |
  <a href="#architecture">Architecture</a> |
  <a href="#documentation">Documentation</a> |
  <a href="#roadmap">Roadmap</a>
</p>

## Overview

Nodo is an experimental C++20 blockchain protocol foundation focused on making security work measurable, state auditable, economics controlled, and finalized history rebuildable.

The current repository is not a production mainnet. It contains a working localnet runtime, testnet-candidate foundations, strict storage/reload checks, P2P transport foundations, treasury execution evidence, governance vote evidence, and extensive tests. Mainnet remains intentionally blocked until custody, networking, economics, storage, and operational safety have been audited and hardened.

## Why Nodo

Many blockchain systems treat protection as background infrastructure. Nodo treats protection as a protocol concern: validators, peers, treasury actions, governance decisions, rewards, penalties, storage, reload, and finality should leave evidence that another node can verify later.

The design target is simple:

- state should be rebuilt from history;
- balances should have origin;
- monetary changes should be authorized;
- treasury spends should be policy checked;
- governance decisions should be vote-evidence backed;
- penalties should require evidence;
- rewards should be tied to measurable protection work.

## Core Principles

Nodo follows the Proof-of-Protection rule set:

| Principle | Meaning |
| --- | --- |
| No inflation without authorization. | Monetary expansion must be explicit and auditable. |
| No balance without origin. | Account state must trace back to genesis, mint, transfer, reward, or slash history. |
| No treasury spend without policy validation. | Treasury execution must satisfy limits, timelocks, approval, balance, and epoch checks. |
| No treasury approval without governance evidence. | Approvals must be reproduced from verified governance lifecycle records. |
| No governance decision without verifiable vote evidence. | Votes, tally, and decision must rebuild deterministically. |
| No reward without measurable protection work. | Reward foundations should be tied to auditable network protection. |
| No penalty without verifiable evidence. | Slashing and penalties must be idempotent and evidence-backed. |
| No state accepted if it cannot be rebuilt from history. | Reload and audit reject non-canonical or divergent state. |

See [Proof of Protection](docs/overview/proof-of-protection.md) for the deeper model.

## Features

Implemented foundations include:

- localnet development pipeline with initialization, transaction submission, local PRECOMMIT-backed block production, finalization, reload, and audit;
- CMake-based C++20 build with one test executable per `tests/**/*.cpp`;
- strict storage schema validation and atomic persistence helpers (`AtomicFile` crash-safe writes);
- canonical finalized artifacts with monetary, treasury, governance, validator, and slashing sections;
- authoritative state-transition execution before block votes and unified canonical replay of accounts plus protocol domains into deterministic state/receipts roots, with coin lot ownership validation and CoinLot registry digest included in the state root commitment;
- OpenSSL Ed25519 user signatures and blst BLS12-381 validator signatures;
- BFT consensus with Quorum Certificate (QC) requiring 2/3+ validator weight from PRECOMMIT votes only;
- durable QC persistence: `FinalizedBlockRecordStore` writes each QC proof atomically to `{dataDir}/sync/qc/{height}.qc`, reloads all records at startup, and restores the in-memory `BlockFinalizationRegistry` — making the fast-path `QC_REQUIRED` sync mode functional across restarts;
- P2P message, gossip, loopback, TCP, encrypted peer-channel, sync, and peer-rate-limiter foundations;
- distributed node daemon with transaction gossip relay, block proposal relay with proposer authentication, PREVOTE/PRECOMMIT voting, and finalized artifact QC verification;
- treasury policy, spend validation, execution evidence, and finalized treasury audit;
- governance vote proof, vote evidence, vote-set audit, tally, decision audit, lifecycle persistence, and lifecycle-backed treasury approval;
- immediate slashing evidence capture for conflicting validator votes, validator penalty decisions, validator lifecycle, and containment-policy foundations;
- testnet-candidate readiness and operator diagnostics foundations.

## Current Status

| Area | Status |
| --- | --- |
| Localnet runtime | Implemented for development and testing. |
| Testnet candidate | Foundations exist; safety gates and diagnostics are active. |
| Mainnet | Blocked by design. Not suitable for production use. |
| QC persistence | Fully implemented; QC proofs survive node restart. |
| Block sync | Fast-path (`QC_REQUIRED`) and persistent-path both implemented and tested. |
| P2P networking | Real socket/gossip foundations exist; live distributed consensus is in progress (Phase 2). |
| Keys and custody | Local development keys exist; production custody is not ready. |
| Governance | Vote evidence and lifecycle audit foundations exist; public governance workflow is still in development. |
| Treasury | Evidence-backed execution validation exists; production operator process is still in development. |
| Economics | Monetary, reward, protection, penalty, and supply-audit foundations exist; CoinLot validation wired into block preview and state root; final economic activation is not complete. |

## Quick Start

### Windows PowerShell

```powershell
$env:BLST_ROOT="$env:USERPROFILE\.nodo\deps\blst"
.\scripts\cmake_build.bat
.\scripts\cmake_test_all.bat
.\build\nodo.exe help
```

### Linux, macOS, Git Bash, or MSYS2

```bash
export BLST_ROOT="$HOME/.nodo/deps/blst"
./scripts/cmake_build.sh
./scripts/cmake_test_all.sh
./build/nodo help
```

If `blst` is not installed, see [Build](docs/getting-started/build.md).

## Build

Prerequisites:

- CMake 3.20 or newer;
- a C++20 compiler;
- OpenSSL libcrypto development files;
- external `blst` headers and library, installed outside this repository.

Windows:

```powershell
.\scripts\cmake_build.bat
```

Linux/macOS/MSYS2:

```bash
./scripts/cmake_build.sh
```

The binary is written to:

```text
build/nodo.exe   # Windows
build/nodo       # Unix-like environments
```

## Run Tests

Windows:

```powershell
.\scripts\cmake_test_all.bat
```

Linux/macOS/MSYS2:

```bash
./scripts/cmake_test_all.sh
```

CTest can also be run directly:

```bash
ctest --test-dir build/cmake --output-on-failure
```

## CLI / Node Commands

Common localnet flow:

```bash
build/nodo init --network localnet --data-dir .nodo
build/nodo keys create --network localnet --data-dir .nodo
build/nodo tx submit --data-dir .nodo
build/nodo block produce --data-dir .nodo
build/nodo node reload --network localnet --data-dir .nodo
build/nodo chain audit --data-dir .nodo
build/nodo status --data-dir .nodo
build/nodo diagnostics --network localnet --data-dir .nodo
```

Network profiles:

- `localnet`: development runtime path;
- `testnet-candidate`: official pre-testnet profile with safety gates;
- `mainnet`: intentionally blocked.

More commands are documented in [CLI](docs/getting-started/cli.md).

## Project Structure

| Path | Purpose |
| --- | --- |
| `apps/cli/` | CLI executable entrypoint. |
| `include/` | Public headers for protocol, runtime, economics, storage, P2P, and utilities. |
| `src/app/` | Command-line orchestration and local operator flows. |
| `src/core/` | Blocks, transactions, account state, validators, and state-transition foundations. |
| `src/consensus/` | Votes, rounds, quorum certificates, finalization, and proposer scheduling. |
| `src/economics/` | Monetary policy, treasury, governance, protection rewards, supply audit, and penalties. |
| `src/node/` | Runtime, storage/reload, finalized artifacts, QC persistence, diagnostics, readiness, and chain audit. |
| `src/p2p/` | Messages, gossip, TCP/loopback transport, sync, encryption, and peer limiting. |
| `src/storage/` | Atomic files, block storage, evidence stores, and persistence helpers. |
| `tests/` | CTest-discovered module tests (one executable per file, named `{module}_{TestFile}`). |
| `scripts/` | Build, test, cleanup, and dependency helper scripts. |
| `docs/` | Project documentation. |

### Storage Layout

```text
{dataDirectory}/
├── manifest               — latest height, hash, and state root
├── schema                 — storage schema version
├── blocks/                — finalized block artifact files
├── mempool/               — persistent mempool transactions
├── sync/
│   ├── checkpoint.conf    — block sync checkpoint (last synced height)
│   └── qc/
│       ├── 1.qc           — FinalizedBlockRecord (QC proof) for height 1
│       ├── 2.qc           — FinalizedBlockRecord (QC proof) for height 2
│       └── ...
└── ...
```

Each `.qc` file is written atomically via temp file + rename. On startup, all `.qc` files are loaded and the in-memory `BlockFinalizationRegistry` is restored before sync or consensus resumes.

## Architecture

```mermaid
flowchart TD
    CLI["CLI / App"] --> Runtime["Node Runtime"]
    Runtime --> Storage["Storage and Reload"]
    Runtime --> Core["Core State Transition"]
    Runtime --> Consensus["Consensus and Finality"]
    Runtime --> P2P["P2P Foundations"]
    Consensus --> QCStore["FinalizedBlockRecordStore\n(sync/qc/{height}.qc)"]
    QCStore --> SyncPath["Block Sync\n(QC_REQUIRED fast-path)"]
    Core --> Economics["Economics"]
    Economics --> Treasury["Treasury Evidence"]
    Economics --> Governance["Governance Lifecycle"]
    Storage --> Audit["Reload and Chain Audit"]
    Governance --> Treasury
```

Read [Architecture Overview](docs/architecture/architecture-overview.md) and [Module Map](docs/architecture/module-map.md).

### QC Persistence Flow

`FinalizedBlockRecord` (containing the BLS12-381 Quorum Certificate) is persisted from four entry points and reloaded on startup:

```text
1. Consensus-driven finalization
   ConsensusEventLoop → setFinalizedCallback
     └── persistFinalizedRecord()    → sync/qc/{height}.qc

2. Fast-path block sync (QC_REQUIRED)
   BlockSyncHandler::applyResponses
     └── finalizationRegistry.recordForHeight()
     └── persistFinalizedRecord()    → sync/qc/{height}.qc

3. Persistent-path batch sync
   PersistentBlockStateSyncApplier::applyValidatedBatch
     └── deserialize serializedFinalizedRecord from batch item
     └── persistFinalizedRecord()    → sync/qc/{height}.qc

4. Gossip-received finalized artifact
   NodeDaemon::processFinalizedArtifacts
     └── FinalizedBlockRecord::deserialize + verify QC
     └── FinalizedBlockRecordStore::save()  → sync/qc/{height}.qc

Startup reload
   NodeOrchestrator::initOrLoad
     └── FinalizedBlockRecordStore::loadAll()
     └── BlockFinalizationRegistry::registerFinalizedBlock() (per record)
```

Sync responses built by `buildSyncResponseBatch()` include the serialized QC for each block, so receiving peers can verify finality without contacting a third party.

### Daemon and Gossip Flow

`NodeDaemon` wraps `NodeOrchestrator` and adds a tick-driven gossip processing layer:

```text
NodeDaemon.tick()
  ├── NodeOrchestrator.tick()          — transport I/O, peer heartbeats, block sync
  ├── processTransactionGossip()       — drain TRANSACTION_GOSSIP inbox
  │     ├── SeenTransactionCache       — LRU+TTL dedup by payloadHash
  │     ├── PersistentMempoolStore::deserializeGossip()          — decode + Ed25519 verify
  │     ├── TransactionAdmissionValidator                       — account + domain admission
  │     └── gossipBroadcast()          — relay if newly admitted
  └── processFinalizedArtifacts()      — drain FINALIZED_BLOCK_ARTIFACT inbox
        ├── FinalizedBlockRecord::deserialize()
        ├── record.verify()            — QC check vs. local validator registry
        ├── FinalizationRegistry::registerFinalizedBlock()
        └── FinalizedBlockRecordStore::save()   — persist QC to disk
```

`ConsensusEventLoop` runs in a background thread inside `NodeOrchestrator`. It validates `BLOCK_PROPOSAL` messages, retains the active candidate outside the canonical chain, accumulates `PREVOTE` and `PRECOMMIT` `VALIDATOR_VOTE` messages, assembles a `QuorumCertificate` only from PRECOMMIT votes, and delegates the only distributed-network append to `BlockFinalizer` after quorum. The local `block produce` command uses a DEVELOPMENT_LOCAL-only helper and is rejected on testnet-candidate and production network classes.

## Documentation

Start with [docs/README.md](docs/README.md).

Key entry points:

- [Project Overview](docs/overview/project-overview.md)
- [Proof of Protection](docs/overview/proof-of-protection.md)
- [Quick Start](docs/getting-started/quick-start.md)
- [Build](docs/getting-started/build.md)
- [Testing](docs/getting-started/testing.md)
- [Architecture Overview](docs/architecture/architecture-overview.md)
- [Persistent Block State Sync](docs/PERSISTENT_BLOCK_STATE_SYNC.md)
- [Governance Vote Evidence](docs/governance/vote-evidence.md)
- [Treasury Execution Evidence](docs/treasury/treasury-execution-evidence.md)
- [Security Model](docs/security/security-model.md)
- [Roadmap](docs/ROADMAP.md)

## Roadmap

Completed foundations:

- localnet runtime pipeline;
- finalized artifact persistence and reload audit;
- monetary reports and supply audit foundations;
- treasury execution evidence;
- governance vote evidence and lifecycle audit;
- P2P transport/gossip/encrypted channel foundations;
- testnet-candidate readiness diagnostics;
- distributed node daemon: transaction gossip, block proposal relay, proposer authentication, finalized artifact QC verification;
- durable QC persistence (`FinalizedBlockRecordStore`): QC proofs survive restart, fast-path `QC_REQUIRED` sync is fully functional, sync responses carry QC proofs to peers;
- signed consensus-vote recovery: `ConsensusRecoveryStore` persists exact signed PREVOTE/PRECOMMIT records so restart can safely resubmit/rebroadcast the same votes without double-voting;
- immediate conflict-to-evidence admission: when live vote collection rejects a same-validator same-height same-round conflict, the event loop builds, verifies, persists and gossips the double-vote evidence in that same tick instead of relying on a later scanner.

In progress:

- live distributed consensus (Phase 2): proposer selection wired to daemon, networked prevote/precommit, view change and recovery hardening around persisted signed votes;
- official testnet runtime hardening;
- production key safety and custody boundaries;
- governance lifecycle transitions;
- validator reward settlement and protection scoring;
- network hardening and peer operations.

Planned:

- audited wallet/custody integration;
- staking-backed governance and validator economics;
- full production slashing lifecycle;
- mainnet readiness gates and external audit process.

See [Roadmap](docs/ROADMAP.md).

## Security

Nodo is security-focused but not suitable for production use. Do not use the current code as a mainnet, custody, treasury, or production validator system unless a future release explicitly says those paths are ready and audited.

Security documentation:

- [SECURITY.md](SECURITY.md)
- [Security Model](docs/security/security-model.md)
- [Threat Model](docs/security/threat-model.md)
- [Key Management](docs/security/key-management.md)

## Contributing

Read [CONTRIBUTING.md](CONTRIBUTING.md) and [Development Guide](docs/development/contributing.md).

Contribution expectations:

- build and test before proposing code changes;
- do not disable tests to hide failures;
- do not weaken protocol, economics, or security validation;
- keep comments in English and useful;
- prefer small, auditable changes.

## License

No repository license file is currently present. Until a license is added, do not assume open-source redistribution rights beyond what GitHub access permits.
