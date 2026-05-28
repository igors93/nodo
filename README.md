# Nodo

**Nodo** is an experimental blockchain project written in C and C++ with a strong focus on deterministic accounting, traceable coin creation, economic security, privacy-oriented ledger design, and long-term cryptographic flexibility.

Nodo is being built step by step as an educational but serious blockchain foundation. The project intentionally prioritizes correctness, auditability, deterministic behavior, and security boundaries before adding networking, production cryptography, validator consensus, or smart contracts.

## Vision

Nodo explores a blockchain architecture where coins are not only simple account balances. Each coin group can have an auditable origin, traceable movement, and eventually a privacy-preserving accounting representation.

The long-term vision is to build a blockchain where:

- every created coin has a registered origin;
- public balances can be rebuilt from accepted chain history;
- private accounting records can be anchored into public blocks;
- private commitments and nullifiers can support privacy without uncontrolled money creation;
- locked coins may contribute to network security;
- validators are economically incentivized to defend the network;
- monetary expansion is rule-based and auditable;
- cryptographic algorithms can evolve over time;
- post-quantum cryptography can be introduced without rewriting the whole system.

## Current Status

Nodo currently implements the first foundations of a blockchain ledger and a privacy-accounting subsystem.

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
- shared deterministic field codec foundation for safer parsing boundaries;
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
16. rebuild the private accounting ledger from blockchain history.

## Core Principles

### 1. Chain History Is the Source of Truth

Nodo does not treat a saved balance as the final truth. Public state must be rebuilt from accepted blockchain history.

```text
Blockchain -> Blocks -> LedgerRecords -> Public State
```

The same principle is now starting to apply to private accounting metadata:

```text
Blockchain -> PRIVATE_ACCOUNTING LedgerRecords -> PrivateAccountingLedger
```

### 2. Coins Have Origin

Every newly created NODO coin must come from a valid mint record.

```text
MintRecord -> LedgerRecord -> Block -> Blockchain
```

This makes supply creation auditable.

### 3. Transactions Do Not Directly Modify State

Transactions are requests. They must become accepted ledger records and be included in blocks before they affect reconstructed state.

```text
Transaction -> LedgerRecord -> Block -> Blockchain -> State
```

### 4. CoinLots Prevent Blind Balance Accounting

Nodo uses `CoinLot`s instead of only increasing or decreasing balances.

When a transfer happens, source lots are marked as `SPENT`, and new output lots are created for:

- recipient;
- fee pool;
- sender change.

This makes coin movement more traceable and helps prevent accidental double-spending inside the state engine.

### 5. Account Nonces Protect Against Replay

Each account tracks the next expected nonce.

If an account has already used nonce `1`, the next accepted transaction must use nonce `2`.

This protects against replay-like errors where two different transactions attempt to use the same sender nonce.

### 6. Locked Coins Cannot Be Spent

Coin lots locked for future economic security must not be spendable while locked.

This foundation prepares Nodo for staking-like security mechanics and validator incentives.

### 7. Privacy Must Still Be Auditable

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

### 8. Serialization Must Be Deterministic

Nodo currently uses deterministic text serialization for development.

A shared `FieldCodec` has been introduced to centralize parsing helpers and reduce duplicated parsing logic.

This is not the final serialization format. Future versions should evolve toward a stricter canonical format, such as a binary encoding or a formally specified deterministic serialization layer.

### 9. Crypto Agility

Nodo is designed so the blockchain is not permanently tied to one signature algorithm.

The current development build uses fake development signatures for architecture testing only.

Future versions should add real signature providers such as:

- Ed25519 or ECDSA for classical signatures;
- ML-DSA or SLH-DSA for post-quantum signatures;
- hybrid signature bundles for critical operations.

## Architecture Overview

```text
nodo/
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
│   │   └── FieldCodec.hpp
│   │
│   ├── staking/
│   │   └── SecurityWeight.hpp
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
│   │   ├── Account.cpp
│   │   ├── Block.cpp
│   │   ├── Blockchain.cpp
│   │   ├── ChainStateRebuilder.cpp
│   │   ├── CoinLot.cpp
│   │   ├── LedgerRecord.cpp
│   │   ├── State.cpp
│   │   └── Transaction.cpp
│   │
│   ├── crypto/
│   │   ├── CryptoAlgorithm.cpp
│   │   ├── CryptoPolicy.cpp
│   │   ├── PrivateKey.cpp
│   │   ├── PublicKey.cpp
│   │   ├── Signature.cpp
│   │   ├── SignatureBundle.cpp
│   │   └── hash.c
│   │
│   ├── economics/
│   │   └── MintRecord.cpp
│   │
│   ├── privacy/
│   │   ├── NullifierSet.cpp
│   │   ├── PrivacyCommitment.cpp
│   │   ├── PrivacyNullifier.cpp
│   │   ├── PrivateAccountingLedger.cpp
│   │   ├── PrivateAccountingLedgerRebuilder.cpp
│   │   └── PrivateAccountingRecord.cpp
│   │
│   ├── serialization/
│   │   └── FieldCodec.cpp
│   │
│   ├── staking/
│   │   └── SecurityWeight.cpp
│   │
│   └── utils/
│       ├── Amount.cpp
│       └── Time.cpp
│
├── scripts/
│   ├── build.bat
│   ├── build.sh
│   ├── clean.bat
│   └── clean.sh
│
├── build/
├── README.md
└── .gitignore
```

## Build

Nodo supports both Linux-style shell builds and Windows builds.

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

### Linux / MSYS2 / Git Bash

```sh
./scripts/clean.sh
./scripts/build.sh
./build/nodo
```

## Expected Demo Output

The current demo should show:

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

Nodo private accounting ledger rebuild executed successfully.
```

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
- public state reconstruction from chain history;
- private ledger reconstruction from chain history;
- private nullifier duplicate protection;
- private commitment duplicate protection.

However, the following areas are still incomplete:

- cryptographic signatures are development-only;
- the current hash implementation is not production-grade;
- serialization is still development text serialization;
- private accounting does not yet use real zero-knowledge proofs;
- commitments and nullifiers are development models;
- storage is not implemented yet;
- networking is not implemented yet;
- validator consensus is not implemented yet;
- slashing and reward rules are not finalized.

Do not use Nodo for real funds.

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
- [ ] Move all legacy parsers into serialization module
- [ ] Add deterministic serialization tests
- [ ] Add round-trip tests for every ledger object
- [ ] Define canonical serialization rules
- [ ] Evaluate binary canonical encoding

### Phase 4: Storage

- [ ] Block file format
- [ ] Blockchain persistence
- [ ] Load chain from disk
- [ ] Validate loaded chain
- [ ] Rebuild public state from stored blocks
- [ ] Rebuild private ledger from stored blocks

### Phase 5: Real Cryptography

- [ ] Replace development hash with production-grade hash
- [ ] Add real signature provider
- [ ] Add deterministic address derivation
- [ ] Add key management boundary
- [ ] Prepare post-quantum provider interfaces

### Phase 6: Private Proof System

- [ ] Commitment tree
- [ ] Nullifier registry
- [ ] Range proof interface
- [ ] Zero-knowledge proof verifier interface
- [ ] Private transfer verification
- [ ] Private burn verification
- [ ] Private mint policy verification

### Phase 7: Economic Security

- [ ] LockPolicy
- [ ] StakePosition
- [ ] Validator
- [ ] ValidatorSet
- [ ] RewardPolicy
- [ ] MonetaryPolicy
- [ ] Slashing rules
- [ ] Proof-of-Locked-Security prototype

### Phase 8: Network

- [ ] Peer protocol
- [ ] Block gossip
- [ ] Transaction gossip
- [ ] Chain synchronization
- [ ] Node validation rules

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

## Disclaimer

Nodo is experimental software under active development.

It is not production-ready and must not be used to store, transfer, or secure real financial value.

The project is currently focused on building a strong architectural foundation before adding networking, real cryptography, validator consensus, production privacy, or economic rewards.
