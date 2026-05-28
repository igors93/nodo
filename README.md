# Nodo

**Nodo** is an experimental C/C++ blockchain project focused on deterministic accounting, auditable coin creation, privacy-oriented ledger design, disk persistence, testable storage reconstruction, and long-term cryptographic agility.

Nodo is being built step by step as an educational but serious blockchain foundation. The project intentionally prioritizes correctness, auditability, deterministic behavior, secure boundaries, and maintainability before adding networking, production cryptography, validator consensus, or smart contracts.

> **Warning**
>
> Nodo is experimental software. It is not production-ready and must not be used to store, transfer, or secure real financial value.

---

## Vision

Nodo explores a blockchain architecture where coins are not treated only as simple account balances. Each coin group can have an auditable origin, traceable movement, and eventually a privacy-preserving accounting representation.

The long-term vision is to build a blockchain where:

- every created coin has a registered origin;
- public balances can be rebuilt from accepted chain history;
- private accounting records can be anchored into public blocks;
- private commitments and nullifiers can support privacy without uncontrolled money creation;
- loaded state can be rebuilt from persisted chain data;
- locked coins may contribute to network security;
- validators are economically incentivized to defend the network;
- monetary expansion is rule-based and auditable;
- cryptographic algorithms can evolve over time;
- post-quantum cryptography can be introduced without rewriting the whole system.

---

## Current Status

Nodo currently implements the first foundations of a blockchain ledger, a privacy-accounting subsystem, deterministic serialization boundaries, disk storage, storage loading, automated tests, and GitHub Actions CI.

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
- LedgerRecord deserialization codec;
- Block snapshot deserialization codec;
- block file storage;
- chain manifest storage;
- block storage index;
- blockchain storage reader;
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

## Core Principles

### 1. Chain History Is the Source of Truth

Nodo does not treat a saved balance as the final truth. Public state must be rebuilt from accepted blockchain history.

```text
Blockchain -> Blocks -> LedgerRecords -> Public State
```

The same principle applies to private accounting metadata:

```text
Blockchain -> PRIVATE_ACCOUNTING LedgerRecords -> PrivateAccountingLedger
```

### 2. Persisted Storage Must Be Verified

A stored file is not trusted just because it exists on disk.

Nodo storage currently validates:

```text
chain_manifest.nodo
        ↓
block_index.nodo
        ↓
block snapshots
        ↓
BlockCodec
        ↓
BlockchainLoader
        ↓
Blockchain validation
```

### 3. Coins Have Origin

Every newly created NODO coin must come from a valid mint record.

```text
MintRecord -> LedgerRecord -> Block -> Blockchain
```

This makes supply creation auditable.

### 4. Transactions Do Not Directly Modify State

Transactions are requests. They must become accepted ledger records and be included in blocks before they affect reconstructed state.

```text
Transaction -> LedgerRecord -> Block -> Blockchain -> State
```

### 5. CoinLots Prevent Blind Balance Accounting

Nodo uses `CoinLot`s instead of only increasing or decreasing balances.

When a transfer happens, source lots are marked as `SPENT`, and new output lots are created for:

- recipient;
- fee pool;
- sender change.

This makes coin movement more traceable and helps prevent accidental double-spending inside the state engine.

### 6. Account Nonces Protect Against Replay

Each account tracks the next expected nonce.

If an account has already used nonce `1`, the next accepted transaction must use nonce `2`.

This protects against replay-like errors where two different transactions attempt to use the same sender nonce.

### 7. Locked Coins Cannot Be Spent

Coin lots locked for future economic security must not be spendable while locked.

This foundation prepares Nodo for staking-like security mechanics and validator incentives.

### 8. Privacy Must Still Be Auditable

Nodo's privacy direction is based on a simple rule:

```text
Privacy must not mean unverifiable money creation.
```

The current privacy architecture uses development versions of:

- `PrivacyCommitment`;
- `PrivacyNullifier`;
- `NullifierSet`;
- `PrivateAccountingRecord`;
- `PrivateAccountingLedger`;
- `PrivateAccountingLedgerRebuilder`.

In the future, these must be backed by real cryptographic commitments, range proofs, nullifier derivation, and zero-knowledge proof verification.

### 9. Serialization Must Be Deterministic

Nodo currently uses deterministic text serialization for development.

The current serialization layer includes:

- `FieldCodec`;
- `LedgerRecordCodec`;
- `BlockCodec`.

This is not the final serialization format. Future versions should evolve toward a stricter canonical format, such as a binary encoding or a formally specified deterministic serialization layer.

### 10. Crypto Agility

Nodo is designed so the blockchain is not permanently tied to one signature algorithm.

The current development build uses fake development signatures for architecture testing only.

Future versions should add real signature providers such as:

- Ed25519 or ECDSA for classical signatures;
- ML-DSA or SLH-DSA for post-quantum signatures;
- hybrid signature bundles for critical operations.

---

## Architecture Overview

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
├── include/
│   ├── app/
│   │   └── DemoScenario.hpp
│   │
│   ├── core/
│   │   ├── Account.hpp
│   │   ├── Block.hpp
│   │   ├── Blockchain.hpp
│   │   ├── ChainStateRebuilder.hpp
│   │   ├── CoinLot.hpp
│   │   ├── LedgerRecord.hpp
│   │   ├── State.hpp
│   │   ├── Transaction.hpp
│   │   └── TransactionType.hpp
│   │
│   ├── crypto/
│   │   ├── CryptoAlgorithm.hpp
│   │   ├── CryptoPolicy.hpp
│   │   ├── PrivateKey.hpp
│   │   ├── PublicKey.hpp
│   │   ├── Signature.hpp
│   │   ├── SignatureBundle.hpp
│   │   └── hash.h
│   │
│   ├── economics/
│   │   └── MintRecord.hpp
│   │
│   ├── privacy/
│   │   ├── NullifierSet.hpp
│   │   ├── PrivacyCommitment.hpp
│   │   ├── PrivacyNullifier.hpp
│   │   ├── PrivateAccountingLedger.hpp
│   │   ├── PrivateAccountingLedgerRebuilder.hpp
│   │   └── PrivateAccountingRecord.hpp
│   │
│   ├── serialization/
│   │   ├── BlockCodec.hpp
│   │   ├── FieldCodec.hpp
│   │   └── LedgerRecordCodec.hpp
│   │
│   ├── staking/
│   │   └── SecurityWeight.hpp
│   │
│   ├── storage/
│   │   ├── BlockFileStore.hpp
│   │   ├── BlockchainLoader.hpp
│   │   ├── BlockchainStorageReader.hpp
│   │   ├── BlockSnapshotHeader.hpp
│   │   ├── BlockStorageIndex.hpp
│   │   └── ChainManifest.hpp
│   │
│   └── utils/
│       ├── Amount.hpp
│       └── Time.hpp
│
├── src/
│   ├── app/
│   │   └── DemoScenario.cpp
│   │
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
│   │   └── SerializationRoundTripTests.cpp
│   │
│   └── storage/
│       └── BlockchainStorageIntegrationTests.cpp
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

## Build

Nodo supports both Linux-style shell builds and Windows builds.

### Fedora / Linux

Requirements:

- `gcc`;
- `g++`;
- C++20 support;
- Bash.

On Fedora:

```sh
sudo dnf install -y gcc gcc-c++ make
```

Make scripts executable:

```sh
chmod +x scripts/*.sh
```

Build and run:

```sh
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

If you use MSYS2 UCRT64, make sure this folder is available in your Windows PATH:

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

### Expected test output

```text
Nodo unified test runner
------------------------

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

The CI currently runs on:

- pushes to `main`;
- pull requests targeting `main`;
- manual workflow dispatch.

The workflow:

1. checks out the repository;
2. installs the compiler toolchain;
3. prints compiler versions;
4. makes shell scripts executable;
5. builds Nodo;
6. runs the demo;
7. runs all tests.

This gives contributors quick feedback when a change breaks build, serialization, storage loading, or state reconstruction.

---

## Storage Layout

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

Current storage components:

- `BlockFileStore` writes deterministic block snapshots;
- `ChainManifest` summarizes the persisted chain;
- `BlockStorageIndex` maps block heights and hashes to snapshot files;
- `BlockchainStorageReader` audits storage metadata and snapshots;
- `BlockSnapshotHeader` validates stored block headers;
- `LedgerRecordCodec` reconstructs ledger records;
- `BlockCodec` reconstructs blocks;
- `BlockchainLoader` reconstructs a complete blockchain from disk.

---

## Expected Demo Output

The current demo should include:

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

Nodo Blockchain loader foundation executed successfully.
```

---

## Current Security Notes

Nodo is still early-stage experimental software.

The current code includes several protective foundations:

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
- centralized field codec foundation;
- controlled LedgerRecord deserialization;
- controlled Block deserialization;
- public state reconstruction from chain history;
- private ledger reconstruction from chain history;
- private nullifier duplicate protection;
- private commitment duplicate protection;
- storage manifest validation;
- storage index validation;
- block snapshot validation;
- loaded-chain validation;
- storage tamper rejection test.

However, the following areas are still incomplete:

- cryptographic signatures are development-only;
- the current hash implementation is not production-grade;
- serialization is still development text serialization;
- private accounting does not yet use real zero-knowledge proofs;
- commitments and nullifiers are development models;
- networking is not implemented yet;
- validator consensus is not implemented yet;
- slashing and reward rules are not finalized;
- the storage format is not yet a final canonical production format.

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
- [x] LedgerRecordCodec
- [x] BlockCodec
- [x] Deterministic serialization tests
- [x] Serialization round-trip tests
- [ ] Move all remaining legacy parsers into serialization module
- [ ] Define canonical serialization rules
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
- [ ] Add CI badge
- [ ] Add contribution guide
- [ ] Add issue templates

### Phase 6: Real Cryptography

- [ ] Replace development hash with production-grade hash
- [ ] Add real signature provider
- [ ] Add deterministic address derivation
- [ ] Add key management boundary
- [ ] Prepare post-quantum provider interfaces

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
- economic security research.

Before contributing, please keep these rules in mind:

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

## Disclaimer

Nodo is experimental software under active development.

It is not production-ready and must not be used to store, transfer, or secure real financial value.

The project is currently focused on building a strong architectural foundation before adding networking, real cryptography, validator consensus, production privacy, or economic rewards.
