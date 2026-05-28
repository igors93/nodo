# Nodo

**Nodo** is an experimental blockchain project written in C and C++ with a strong focus on security, economic incentives, traceable coin creation, deterministic state reconstruction, and long-term cryptographic flexibility.

Nodo is being built step by step as an educational yet serious blockchain foundation. The project intentionally avoids rushing into networking, validators, consensus, storage, or smart contracts before the core ledger model is deterministic, auditable, and safe.

> **Warning:** Nodo is experimental software. It is not production-ready and must not be used to store, transfer, or secure real financial value.

---

## Vision

Nodo explores a blockchain architecture where coins are not treated as simple account balances. Each coin group is represented as a traceable `CoinLot`, created from an auditable `MintRecord`, moved through validated `Transaction`s, stored as `LedgerRecord`s, grouped inside `Block`s, and reconstructed from accepted `Blockchain` history.

The long-term vision is to build a blockchain where:

- every coin has a registered origin;
- coin movement is traceable;
- locked coins can contribute to network security;
- validators are economically incentivized to defend the network;
- monetary expansion is controlled, auditable, and rule-based;
- cryptographic algorithms can evolve over time;
- post-quantum cryptography can be introduced without rewriting the whole chain.

---

## Current Status

Nodo currently implements the first foundations of a blockchain ledger:

- deterministic monetary amounts using integer raw units;
- mint records for auditable coin creation;
- traceable coin lots;
- development signature foundation;
- crypto-agile signature model;
- signed transactions;
- ledger records;
- blocks;
- blockchain validation;
- build scripts for Linux-style shells;
- build scripts for Windows CMD / PowerShell;
- chain audit reports;
- state reconstruction from blockchain history;
- mint state reconstruction;
- transfer state reconstruction using CoinLots.

Nodo can currently:

1. create an auditable genesis mint;
2. convert that mint into a ledger record;
3. place the record inside a genesis block;
4. create a signed transfer transaction;
5. convert the transaction into a ledger record;
6. place the transfer record inside a second block;
7. validate the blockchain;
8. rebuild the state from accepted blockchain history;
9. apply a transfer by consuming spendable CoinLots;
10. preserve supply by creating recipient, fee pool, and change CoinLots.

---

## Core Principles

### 1. Chain History Is the Source of Truth

Nodo does not treat a saved balance as the final truth. The state must be reconstructed from accepted blockchain history.

```text
Blockchain -> Blocks -> LedgerRecords -> State
```

### 2. Coins Have Origin

Every newly created NODO coin must come from a valid `MintRecord`.

```text
MintRecord -> CoinLot
```

This makes supply creation auditable.

### 3. Transactions Do Not Directly Modify State

Transactions are requests. They must become accepted ledger records and be included in blocks before they affect the reconstructed state.

```text
Transaction -> LedgerRecord -> Block -> Blockchain -> State
```

### 4. CoinLots Prevent Blind Balance Accounting

Nodo currently uses `CoinLot`s instead of only increasing or decreasing balances.

When a transfer happens, source lots are marked as `SPENT`, and new output lots are created for:

- recipient;
- fee pool;
- sender change.

This makes coin movement traceable and helps prevent accidental double-spending inside the state engine.

### 5. Locked Coins Cannot Be Spent

CoinLots locked for security are not spendable. This is important for Nodo's future economic security model.

### 6. Crypto Agility

Nodo is designed so the blockchain is not permanently tied to one signature algorithm.

The current development build uses a fake development signature for architecture testing only. Future versions should add real signature providers such as:

- Ed25519 or ECDSA for classical signatures;
- ML-DSA or SLH-DSA for post-quantum signatures;
- hybrid signature bundles for critical operations.

---

## Current Architecture

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
│   ├── staking/
│   │   └── SecurityWeight.cpp
│   │
│   └── utils/
│       ├── Amount.cpp
│       └── Time.cpp
│
├── scripts/
│   ├── build.sh
│   ├── clean.sh
│   ├── build.bat
│   └── clean.bat
│
├── build/
├── README.md
└── .gitignore
```

---

## Build

Nodo supports both Linux-style shell builds and Windows builds.

### Linux / MSYS2 / Git Bash

```sh
./scripts/clean.sh
./scripts/build.sh
./build/nodo
```

### Windows CMD / PowerShell

```powershell
.\scripts\clean.bat
.\scripts\build.bat
.\build\nodo.exe
```

### Windows Requirement

If you use MSYS2 UCRT64, make sure this folder is available in your Windows `PATH`:

```text
C:\msys64\ucrt64\bin
```

You can test the compiler with:

```powershell
gcc --version
g++ --version
```

If both commands return a version, the Windows build script should work.

---

## Expected Demo Output

The current demo should show a valid blockchain and a reconstructed state.

Example final balances:

```text
Rebuilt total supply: 1000.00000000 NODO
Rebuilt Igor balance: 974.99900000 NODO
Rebuilt Ana balance: 25.00000000 NODO
Rebuilt fee pool balance: 0.00100000 NODO
Rebuilt supply audit: VALID
```

This means:

- 1000 NODO were minted to Igor;
- Igor sent 25 NODO to Ana;
- 0.001 NODO was preserved as a fee pool output;
- total supply remained auditable.

---

## Current Security Foundations

Nodo is still early-stage software, but the project already includes several protective foundations:

- integer-based monetary amounts;
- duplicate `MintRecord` rejection;
- duplicate transaction application rejection;
- locked `CoinLot`s cannot be spent;
- spent `CoinLot`s cannot be spent again;
- slashed `CoinLot`s cannot be spent;
- block hash validation;
- previous-hash chain validation;
- ledger record payload hashing;
- deterministic serialization;
- state reconstruction from chain history;
- supply audit after state reconstruction.

---

## Current Limitations

The following areas are still experimental or incomplete:

- cryptographic signatures are development-only;
- the hash implementation is not production-grade yet;
- account nonces are not fully enforced through an account model;
- deterministic address generation is not implemented yet;
- storage is not implemented yet;
- networking is not implemented yet;
- validator consensus is not implemented yet;
- slashing and reward rules are not finalized;
- no automated test suite exists yet.

Do not use Nodo for real funds.

---

## Roadmap

### Phase 1: Core Ledger Foundation

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
- [x] Linux build scripts
- [x] Windows build scripts

### Phase 2: State Safety

- [ ] Account model
- [ ] Nonce validation
- [ ] Deterministic address generation
- [ ] Stronger transaction replay protection
- [ ] Transfer tests
- [ ] Chain rebuild tests
- [ ] Supply invariant tests

### Phase 3: Storage

- [ ] Block file format
- [ ] Blockchain persistence
- [ ] Load chain from disk
- [ ] Validate loaded chain
- [ ] Rebuild state from stored blocks

### Phase 4: Real Cryptography

- [ ] Replace educational hash with production-grade hash
- [ ] Add real signature provider
- [ ] Add deterministic address derivation
- [ ] Add key management boundary
- [ ] Prepare post-quantum provider interfaces

### Phase 5: Economic Security

- [ ] LockPolicy
- [ ] StakePosition
- [ ] Validator
- [ ] ValidatorSet
- [ ] RewardPolicy
- [ ] MonetaryPolicy
- [ ] Slashing rules
- [ ] Proof-of-Locked-Security prototype

### Phase 6: Network

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
- test coverage;
- blockchain storage;
- economic security research.

Before contributing, please keep these rules in mind:

1. Security comes before speed.
2. Determinism comes before convenience.
3. State must be rebuildable from chain history.
4. No monetary value should appear without an auditable origin.
5. No transaction should mutate state without becoming part of accepted history.
6. No cryptographic primitive should be invented casually.
7. Code comments and public documentation should be written in English.

---

## Disclaimer

Nodo is experimental software under active development. It is not production-ready and must not be used to store, transfer, or secure real financial value.

The project is currently focused on building a strong architectural foundation before adding networking, real cryptography, validator consensus, or economic rewards.
