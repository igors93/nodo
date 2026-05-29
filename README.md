# Nodo

[![Nodo CI](https://github.com/igors93/nodo/actions/workflows/ci.yml/badge.svg)](https://github.com/igors93/nodo/actions/workflows/ci.yml)

**Nodo** is an experimental C/C++ blockchain foundation focused on deterministic accounting, auditable coin creation, privacy-oriented ledger design, strict serialization, verifiable disk storage, and long-term cryptographic agility.

Nodo is being built step by step as a serious educational blockchain engine. The project currently prioritizes correctness, auditability, deterministic behavior, storage safety, and maintainable architecture before adding networking, validator consensus, production cryptography, smart contracts, or real economic value.

> **Warning**
>
> Nodo is experimental software. It is not production-ready and must not be used to store, transfer, or secure real financial value.

---

## What Nodo Is

Nodo is currently a **local blockchain engine prototype**.

It can create blocks, link them by hash, store them on disk, reload them, validate the chain, rebuild public balances from history, and rebuild private-accounting metadata from accepted blockchain records.

In simple terms:

```text
accepted blockchain history
        ↓
validated blocks
        ↓
ledger records
        ↓
rebuilt public state
        ↓
rebuilt private accounting ledger
```

Nodo is not yet a decentralized public blockchain network. Networking, peer discovery, mempool, validator consensus, production signatures, and real wallet infrastructure are future phases.

---

## Vision

Nodo explores a blockchain architecture where coins are not only simple account balances. Each coin group should have a traceable origin, controlled movement, and eventually a privacy-preserving accounting representation.

Long-term goals:

- every created coin must have an auditable origin;
- public state must be rebuildable from accepted chain history;
- private accounting must not allow hidden inflation;
- private commitments and nullifiers should support privacy without double-spending;
- storage must be verifiable before loading;
- validator security should eventually come from locked economic weight;
- cryptography should be replaceable through versioned provider boundaries;
- post-quantum cryptography should be possible without rewriting the whole system.

---

## Current Status

Nodo currently implements foundational blockchain, storage, serialization, and privacy-accounting components.

Implemented foundations:

- deterministic monetary amounts using integer raw units;
- auditable mint records;
- traceable coin lots;
- account model with nonce validation;
- development signature foundation;
- crypto-agile signature model;
- signed public transactions;
- ledger records;
- blocks;
- blockchain validation;
- public state reconstruction from chain history;
- transfer application using spendable coin lots;
- fee pool accounting;
- privacy commitments;
- privacy nullifiers;
- nullifier set for private double-spend protection;
- private accounting records;
- private accounting ledger;
- private accounting ledger records anchored into blocks;
- private accounting ledger reconstruction from blockchain history;
- deterministic field codec foundation;
- dedicated serialization codecs;
- canonical serialization rules documentation;
- SHA-256 hash provider foundation;
- signature provider boundary;
- deterministic address derivation foundation;
- key management boundary;
- post-quantum provider interfaces;
- audited signature provider integration boundary;
- block file storage;
- chain manifest storage;
- block storage index;
- blockchain storage reader;
- block snapshot header validation;
- blockchain loader foundation;
- storage integration tests;
- unified test runner;
- GitHub Actions CI;
- cross-platform build scripts for Linux-like shells and Windows.

Nodo can currently:

1. create an auditable genesis mint;
2. convert that mint into a public ledger record;
3. place the record inside a genesis block;
4. create a signed transfer transaction;
5. convert the transaction into a ledger record;
6. place the transfer record inside a second block;
7. validate the blockchain;
8. rebuild public state from accepted blockchain history;
9. apply public transfer effects using coin lots;
10. create private commitments;
11. create private nullifiers;
12. reject duplicate private nullifiers;
13. create private accounting records;
14. validate a private accounting ledger;
15. anchor private accounting records into blockchain blocks;
16. rebuild the private accounting ledger from blockchain history;
17. persist blocks to disk;
18. write a chain manifest;
19. write a block storage index;
20. read and validate persisted storage metadata;
21. parse stored block snapshot headers;
22. deserialize persisted ledger records;
23. deserialize persisted blocks;
24. load a complete blockchain from disk;
25. rebuild public state from the loaded blockchain;
26. rebuild private ledger state from the loaded blockchain;
27. run automated serialization and storage integration tests;
28. validate the project in GitHub Actions CI.

---

## Repository Layout

```text
nodo/
├── .github/
│   └── workflows/
│       └── ci.yml
│
├── apps/
│   └── cli/
│       └── main.cpp
│
├── docs/
│   ├── crypto/
│   │   ├── HASH_PROVIDER.md
│   │   ├── SIGNATURE_PROVIDER.md
│   │   ├── ADDRESS_DERIVATION.md
│   │   ├── KEY_MANAGEMENT.md
│   │   ├── POST_QUANTUM_PROVIDER_INTERFACES.md
│   │   └── AUDITED_SIGNATURE_PROVIDER.md
│   └── serialization/
│       └── CANONICAL_SERIALIZATION.md
│
├── include/
│   ├── app/
│   ├── core/
│   ├── crypto/
│   ├── economics/
│   ├── privacy/
│   ├── serialization/
│   ├── staking/
│   ├── storage/
│   └── utils/
│
├── src/
│   ├── app/
│   ├── core/
│   ├── crypto/
│   ├── economics/
│   ├── privacy/
│   ├── serialization/
│   ├── staking/
│   ├── storage/
│   └── utils/
│
├── tests/
│   ├── serialization/
│   └── storage/
│
├── scripts/
│   ├── build.bat
│   ├── build.sh
│   ├── clean.bat
│   ├── clean.sh
│   ├── test_all.bat
│   ├── test_all.sh
│   ├── test_serialization.bat
│   ├── test_serialization.sh
│   ├── test_storage.bat
│   └── test_storage.sh
│
├── data/
├── build/
├── README.md
└── .gitignore
```

---

## Core Principles

### 1. Chain History Is the Source of Truth

Nodo does not treat saved balances as final truth. State must be rebuilt from accepted blockchain history.

```text
Blockchain -> Blocks -> LedgerRecords -> Public State
```

The same rule applies to private accounting:

```text
Blockchain -> PRIVATE_ACCOUNTING LedgerRecords -> PrivateAccountingLedger
```

### 2. Coin Creation Must Be Auditable

Coins must not appear from nowhere.

```text
MintRecord -> LedgerRecord -> Block -> Blockchain
```

Every mint must have a reason, a recipient, an amount, a timestamp, and an accepted place in chain history.

### 3. Transactions Are Requests, Not Direct State Mutations

A transaction does not directly modify state.

```text
Transaction -> LedgerRecord -> Block -> Blockchain -> Rebuilt State
```

This keeps state reconstruction deterministic and auditable.

### 4. CoinLots Prevent Blind Balance Accounting

Nodo uses `CoinLot`s instead of only increasing or decreasing balances.

When a transfer happens:

- source lots are marked as spent;
- a recipient lot is created;
- a fee pool lot is created;
- a sender change lot may be created.

This improves auditability and helps prevent accidental double-spending inside the state engine.

### 5. Nonces Protect Against Replay

Each account tracks the next expected nonce.

If an account already used nonce `1`, the next accepted transaction must use nonce `2`.

### 6. Privacy Must Not Hide Inflation

Nodo's privacy direction follows this rule:

```text
privacy must not mean unverifiable money creation
```

Private accounting records use commitments and nullifiers as development foundations. Future versions must use real cryptographic commitments, range proofs, nullifier derivation, and zero-knowledge proof verification.

### 7. Persisted Storage Is Untrusted Input

Files loaded from disk must be treated as untrusted until verified.

```text
chain_manifest.nodo
        ↓
block_index.nodo
        ↓
block snapshots
        ↓
snapshot header validation
        ↓
BlockCodec
        ↓
BlockchainLoader
        ↓
Blockchain validation
```

### 8. Serialization Must Be Canonical

Blockchain systems require deterministic serialization.

```text
same object
        ↓
same serialization
        ↓
same hash
        ↓
same validation result
```

Nodo's current canonical serialization rules are documented in:

```text
docs/serialization/CANONICAL_SERIALIZATION.md

docs/crypto/HASH_PROVIDER.md

docs/crypto/SIGNATURE_PROVIDER.md

docs/crypto/ADDRESS_DERIVATION.md

docs/crypto/KEY_MANAGEMENT.md

docs/crypto/POST_QUANTUM_PROVIDER_INTERFACES.md

docs/crypto/AUDITED_SIGNATURE_PROVIDER.md
```

---

## Serialization Layer

Nodo now uses dedicated codec boundaries for important objects.

Current codec foundations:

- `FieldCodec`;
- `MintRecordCodec`;
- `PrivacyCommitmentCodec`;
- `PrivacyNullifierCodec`;
- `PrivateAccountingRecordCodec`;
- `LedgerRecordCodec`;
- `BlockCodec`;
- `ChainManifestCodec`;
- `BlockStorageIndexCodec`;
- `BlockSnapshotHeaderCodec`.

Codec responsibility:

```text
serialized text
        ↓
strict parsing
        ↓
object reconstruction
        ↓
object validation
        ↓
round-trip comparison
        ↓
accept or reject
```

The main rule is:

```text
deserialize(serialize(object)).serialize() == serialize(object)
```

If a parsed object serializes back differently, the input is not canonical and must be rejected.

---

## Storage Foundation

The current storage foundation writes development snapshots under:

```text
data/
├── chain_manifest.nodo
├── block_index.nodo
└── blocks/
    ├── block_0_<hash>.nodo
    ├── block_1_<hash>.nodo
    └── block_2_<hash>.nodo
```

Storage components:

- `BlockFileStore` writes deterministic block snapshots;
- `ChainManifest` summarizes the persisted chain;
- `ChainManifestCodec` validates manifest parsing;
- `BlockStorageIndex` maps block heights and hashes to snapshot files;
- `BlockStorageIndexCodec` validates index parsing;
- `BlockchainStorageReader` audits storage metadata and snapshots;
- `BlockSnapshotHeader` validates stored block headers;
- `BlockSnapshotHeaderCodec` extracts and validates header metadata;
- `LedgerRecordCodec` reconstructs ledger records;
- `BlockCodec` reconstructs blocks;
- `BlockchainLoader` reconstructs a complete blockchain from disk.

---

## Build

Nodo supports Linux-style shells and Windows builds.

### Fedora / Linux

Install compiler tools:

```sh
sudo dnf install -y gcc gcc-c++ make
```

Build and run:

```sh
chmod +x scripts/*.sh
./scripts/clean.sh
./scripts/build.sh
./build/nodo
```

### Ubuntu / Debian

```sh
sudo apt-get update
sudo apt-get install -y build-essential
chmod +x scripts/*.sh
./scripts/clean.sh
./scripts/build.sh
./build/nodo
```

### Windows CMD / PowerShell

Requirements:

- MSYS2 UCRT64 or another GCC/G++ toolchain available in PATH;
- `gcc`;
- `g++` with C++20 support.

If using MSYS2 UCRT64, add this folder to your Windows PATH:

```text
C:\msys64\ucrt64\bin
```

Build and run:

```powershell
.\scripts\clean.bat
.\scripts\build.bat
.\build\nodo.exe
```

---

## Tests

Nodo currently includes framework-free test runners.

### Linux / Fedora

Run all tests:

```sh
chmod +x scripts/*.sh
./scripts/test_all.sh
```

Run serialization tests only:

```sh
./scripts/test_serialization.sh
```

Run storage integration tests only:

```sh
./scripts/test_storage.sh
```

### Windows CMD / PowerShell

Run all tests:

```powershell
.\scripts\test_all.bat
```

Run serialization tests only:

```powershell
.\scripts\test_serialization.bat
```

Run storage integration tests only:

```powershell
.\scripts\test_storage.bat
```

Expected output:

```text
Nodo unified test runner
------------------------

Running crypto tests...
Nodo crypto hash tests passed.
Nodo signature provider tests passed.
Nodo address derivation tests passed.
Nodo key management tests passed.
Nodo post-quantum provider interface tests passed.
Nodo audited signature provider tests passed.

Running serialization tests...
Nodo serialization round-trip tests passed.

Running blockchain storage integration tests...
Nodo blockchain storage integration tests passed.

All Nodo tests completed successfully.
```

---

## GitHub Actions CI

Nodo includes a GitHub Actions workflow at:

```text
.github/workflows/ci.yml
```

The workflow runs on:

- pushes to `main`;
- pull requests targeting `main`;
- manual workflow dispatch.

The workflow currently:

1. checks out the repository;
2. installs the compiler toolchain;
3. prints compiler versions;
4. makes shell scripts executable;
5. builds Nodo;
6. runs the demo;
7. runs all tests.

---

## Expected Demo Output

The current demo should include sections similar to:

```text
Full Blockchain validation: VALID

Rebuilt total supply: 1000.00000000 NODO
Rebuilt Igor balance: 974.99900000 NODO
Rebuilt Ana balance: 25.00000000 NODO
Rebuilt fee pool balance: 0.00100000 NODO
Rebuilt supply audit: VALID

Private ledger validation: VALID
Private ledger record count: 2
Private ledger nullifier count: 1
Private ledger commitment count: 3
Private ledger minted supply: 1000.00000000 NODO
Private ledger outstanding supply: 1000.00000000 NODO

Private Accounting Ledger rebuilt from Blockchain.
Rebuilt private ledger validation: VALID

Block storage foundation preview:
Stored block verification: VALID

Chain storage manifest preview:
Manifest validation: VALID
Manifest matches Blockchain: VALID

Block storage index preview:
Storage index validation: VALID
Storage index matches Blockchain: VALID

Blockchain storage reader preview:
Storage reader audit: VALID

Block snapshot header parser preview:
Snapshot header sequence: VALID

LedgerRecord deserialization preview:
Deserialized LedgerRecords match Blockchain: VALID

Block snapshot deserialization preview:
Deserialized blocks match Blockchain: VALID

Blockchain loader foundation preview:
Blockchain loader audit: VALID
Loaded Blockchain validation: VALID
Loaded Blockchain matches original: VALID
Loaded public state supply audit: VALID
Loaded private ledger validation: VALID
```

Exact hashes and timestamps will change between runs.

---

## Current Security Notes

Nodo already includes several protective foundations:

- integer-based monetary amounts;
- duplicate mint record rejection;
- duplicate transaction application rejection;
- account nonce validation;
- locked coin lots cannot be spent;
- spent coin lots cannot be spent again;
- block hash validation;
- previous-hash chain validation;
- ledger record payload hashing;
- deterministic serialization;
- canonical serialization documentation;
- centralized codec boundaries;
- controlled mint record deserialization;
- controlled privacy commitment deserialization;
- controlled privacy nullifier deserialization;
- controlled private accounting record deserialization;
- controlled ledger record deserialization;
- controlled block deserialization;
- controlled chain manifest deserialization;
- controlled block storage index deserialization;
- controlled block snapshot header parsing;
- public state reconstruction from chain history;
- private ledger reconstruction from chain history;
- private nullifier duplicate protection;
- private commitment duplicate protection;
- storage manifest validation;
- storage index validation;
- block snapshot validation;
- loaded-chain validation;
- storage tamper rejection tests.

Still incomplete:

- cryptographic signatures are development-only;
- the current hash boundary uses SHA-256, but still needs audit/provider hardening before real value;
- serialization is still a development text format;
- private accounting does not yet use real zero-knowledge proofs;
- commitments and nullifiers are development models;
- networking is not implemented yet;
- mempool is not implemented yet;
- validator consensus is not implemented yet;
- slashing and reward rules are not finalized;
- the storage format is not yet a final production format.

Do not use Nodo for real funds.

---

## Roadmap

### Phase 1: Public Ledger Foundation

- [x] Amount model
- [x] MintRecord
- [x] CoinLot
- [x] Transaction
- [x] LedgerRecord
- [x] Block
- [x] Blockchain
- [x] Chain audit
- [x] Mint state reconstruction
- [x] Transfer state reconstruction
- [x] Account model
- [x] Nonce validation

### Phase 2: Privacy Accounting Foundation

- [x] PrivacyCommitment
- [x] PrivacyNullifier
- [x] NullifierSet
- [x] PrivateAccountingRecord
- [x] PrivateAccountingLedger
- [x] PRIVATE_ACCOUNTING LedgerRecord
- [x] PrivateAccountingLedgerRebuilder

### Phase 3: Serialization Safety

- [x] FieldCodec boundary
- [x] MintRecordCodec
- [x] PrivacyCommitmentCodec
- [x] PrivacyNullifierCodec
- [x] PrivateAccountingRecordCodec
- [x] LedgerRecordCodec
- [x] BlockCodec
- [x] ChainManifestCodec
- [x] BlockStorageIndexCodec
- [x] BlockSnapshotHeaderCodec
- [x] Deterministic serialization tests
- [x] Serialization round-trip tests
- [x] Move remaining major legacy parsers into serialization module
- [x] Define canonical serialization rules
- [x] Add canonical serialization rejection tests
- [ ] Evaluate binary canonical encoding

### Phase 4: Storage

- [x] Block file store
- [x] Chain manifest
- [x] Block storage index
- [x] Blockchain storage reader
- [x] Block snapshot header parser
- [x] Block snapshot deserializer
- [x] Blockchain loader foundation
- [x] Validate loaded chain
- [x] Rebuild public state from loaded chain
- [x] Rebuild private ledger from loaded chain
- [x] Storage integration tests
- [ ] Define final canonical storage format
- [ ] Add snapshot corruption test matrix
- [ ] Add missing-file test matrix
- [ ] Add manifest/index mismatch test matrix

### Phase 5: Project Automation

- [x] Cross-platform build scripts
- [x] Cross-platform test scripts
- [x] Unified test runner
- [x] GitHub Actions CI
- [x] CI badge
- [ ] Add contribution guide
- [ ] Add issue templates

### Phase 6: Real Cryptography

- [x] Replace development hash with SHA-256 provider foundation
- [x] Add signature provider boundary
- [x] Add audited signature provider integration boundary
- [ ] Connect real audited signature provider implementation
- [x] Add deterministic address derivation
- [x] Add key management boundary
- [x] Prepare post-quantum provider interfaces

### Phase 7: Private Proof System

- [ ] Commitment tree
- [ ] Nullifier registry
- [ ] Range proof interface
- [ ] Zero-knowledge proof verifier interface
- [ ] Private transfer verification
- [ ] Private burn verification
- [ ] Private mint policy verification

### Phase 8: Economic Security

- [ ] LockPolicy
- [ ] StakePosition
- [ ] Validator
- [ ] ValidatorSet
- [ ] RewardPolicy
- [ ] MonetaryPolicy
- [ ] Slashing rules
- [ ] Proof-of-Locked-Security prototype

### Phase 9: Network

- [ ] Peer protocol
- [ ] Mempool
- [ ] Block gossip
- [ ] Transaction gossip
- [ ] Chain synchronization
- [ ] Node validation rules

---

## Contributing

Contributions are welcome, especially in these areas:

- C++ safety and architecture;
- deterministic serialization;
- cryptography provider design;
- ledger validation;
- private accounting research;
- zero-knowledge proof integration;
- blockchain storage;
- test coverage;
- economic security research;
- peer-to-peer network design.

Before contributing, please follow these principles:

1. Security comes before speed.
2. Determinism comes before convenience.
3. State must be rebuildable from chain history.
4. No monetary value should appear without an auditable origin.
5. No transaction should mutate state without becoming part of accepted history.
6. Privacy must not allow hidden inflation.
7. No cryptographic primitive should be invented casually.
8. Development-only cryptography must be clearly marked.
9. Persisted storage must be validated before loading.
10. Tests should be added for any security-sensitive behavior.

---

## Project Maturity

Nodo is currently in the **foundation stage**.

Current maturity:

```text
local blockchain engine: active prototype
disk persistence: implemented foundation
serialization safety: implemented foundation
privacy accounting: development foundation
networking: not implemented
consensus: not implemented
production cryptography: not implemented
wallet infrastructure: not implemented
```

Nodo is not yet a public blockchain network. It is a growing blockchain engine that is being prepared for future networking, consensus, cryptography, and privacy work.

---

## Disclaimer

Nodo is experimental software under active development.

It is not production-ready and must not be used to store, transfer, or secure real financial value.

The project is currently focused on building a strong architectural foundation before adding networking, real cryptography, validator consensus, production privacy, or economic rewards.
