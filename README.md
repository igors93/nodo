# Nodo

[![Nodo CI](https://github.com/igors93/nodo/actions/workflows/ci.yml/badge.svg)](https://github.com/igors93/nodo/actions/workflows/ci.yml)

**Nodo** is an experimental C/C++ blockchain foundation focused on security, information reliability, auditable coin existence, deterministic state reconstruction, validator-based protection, and long-term cryptographic agility.

Nodo is being built as a serious educational blockchain engine. The project currently prioritizes correctness, traceability, deterministic behavior, verifiable storage, and maintainable architecture before adding public networking, validator consensus, wallet infrastructure, or real economic value.

> **Warning**
>
> Nodo is experimental software. It is not production-ready and must not be used to store, transfer, or secure real financial value.

---

## Project Vision

Nodo is designed around one core idea:

```text
The blockchain exists to protect information.
Coins are created as a controlled reward for useful protection work.
Every accepted state must be rebuildable from chain history.
```

Nodo should not be a blockchain where coins simply appear at the beginning and then move around.

The long-term goal is a network where:

- the initial chain can start with zero spendable supply;
- coins are created through validator protection work;
- coin creation is limited by economic rules;
- validators are rewarded for useful work, not for score alone;
- validator score is a public trust signal stored on-chain;
- coin existence is verifiable before a coin can interact with the chain;
- every coin lot has an auditable origin;
- storage, serialization, and cryptography are treated as security boundaries;
- future post-quantum cryptography can be integrated without rewriting the whole project.

---

## Simple Explanation

In simple terms, Nodo should work like this:

```text
Validators protect the blockchain.
The blockchain records who protected it.
The network calculates how much reward is allowed.
Rewards are distributed only for valid work.
Coins born from rewards are recorded on-chain.
Any later use of those coins must prove that the coins exist and were not already spent.
```

Nodo's economic philosophy is:

```text
Coins are not born from nothing.
Coins are born from verified protection work.
Inflation is controlled by rules.
Trust is earned over time.
```

---

## Current Status

Nodo is currently a **local blockchain engine prototype**, not a decentralized public network yet.

The current code already includes foundations for:

- deterministic monetary amounts using integer raw units;
- auditable mint records;
- traceable coin lots;
- account model with nonce validation;
- signed public transactions;
- ledger records;
- blocks;
- blockchain validation;
- public state reconstruction from chain history;
- transfer application using spendable coin lots;
- fee pool accounting;
- privacy commitments;
- privacy nullifiers;
- private double-spend protection foundation;
- private accounting records;
- private accounting ledger;
- private accounting records anchored into blocks;
- private accounting ledger reconstruction from blockchain history;
- dedicated serialization codecs;
- canonical serialization rules;
- SHA-256 hash provider foundation;
- signature provider boundary;
- deterministic address derivation;
- key pair boundary;
- post-quantum provider interfaces;
- audited signature provider integration boundary;
- block file storage;
- chain manifest storage;
- block storage index;
- blockchain storage reader;
- blockchain loader foundation;
- automated tests;
- cross-platform build and test scripts;
- GitHub Actions CI.

The current demo still uses a development genesis mint. The long-term target described in this README is to evolve from that demo model into a protection-based coin creation model.

---

## Core Principles

### 1. Chain History Is the Source of Truth

Nodo must not trust a saved balance just because a file says it exists.

The correct rule is:

```text
accepted blockchain history
        ↓
validated blocks
        ↓
ledger records
        ↓
rebuilt state
```

If a state cannot be rebuilt from accepted history, it should not be trusted.

---

### 2. Coin Existence Must Be Verifiable

A coin should not be treated only as a number inside an account.

Nodo should track coin existence through identifiable coin lots.

A coin lot is a group of spendable units with a known origin.

Example:

```text
CoinLot
- id: unique identifier
- origin: GenesisReward / transfer change / fee distribution
- owner: address
- amount: raw units
- status: unspent or spent
```

Before a coin can interact with the blockchain, validators must be able to verify:

```text
Does this coin lot exist?
Does the sender own it?
Was it already spent?
Is the signature valid?
Does the transaction follow the rules?
```

This protects the chain from hidden inflation and double spending.

---

### 3. Genesis Should Mean Reward Birth, Not Free Premine

Nodo should move toward a model where the first spendable coins are not created as an arbitrary premine.

The preferred long-term model is:

```text
initial chain starts with zero spendable supply
        ↓
validators perform useful protection work
        ↓
the epoch calculates allowed rewards
        ↓
GenesisReward records create new coin lots
```

In this model, "genesis" is not just the first block. It also means the birth certificate of newly created reward coins.

Recommended concept name:

```text
GenesisReward
```

A `GenesisReward` should record:

- epoch number;
- validator address;
- amount created;
- reason for creation;
- related work records;
- reward policy version;
- block hash where it was accepted.

---

### 4. Validators Are Paid for Useful Work, Not for Score Alone

Validator score should not create money by itself.

The score should mean:

```text
How much can the network trust this validator?
```

The reward should come from useful work:

```text
What protection work did this validator perform for the network?
```

Correct idea:

```text
high score = more trust / more chance to be selected
valid work = reward eligibility
```

Wrong idea:

```text
high score = automatic passive income forever
```

Nodo should reward validators for actions such as:

- validating transactions;
- checking whether coin lots exist;
- checking whether coin lots were already spent;
- verifying signatures;
- validating blocks;
- responding to integrity challenges;
- helping reconstruct state;
- serving old blocks or storage proofs;
- participating honestly in consensus.

---

### 5. Validator Score Is On-Chain Trust

Validator score should be recorded in the blockchain.

The score should be limited from 0 to 100.

Example:

```text
ValidatorScoreRecord
- validatorAddress
- epoch
- previousScore
- newScore
- reason
- evidenceHash
```

The score should increase slowly and decrease quickly when there is bad behavior.

Score should be affected by:

- correct validation history;
- uptime;
- response to challenges;
- no conflicting signatures;
- no repeated invalid work;
- no suspicious cluster behavior;
- no double-signing;
- no known protocol violations.

Important rule:

```text
Score is trust, not money.
Work is what earns reward.
```

---

### 6. Epochs Control Reward and Inflation

Nodo should use protection epochs.

An epoch is a time or block interval where the network measures work and calculates rewards.

Example:

```text
ProtectionEpoch
- epoch number
- start block
- end block
- total work accepted
- fees collected
- maximum new emission
- final reward pool
```

The epoch should decide:

- how much valid work happened;
- how many fees were collected;
- how much new emission is allowed;
- which validators performed valid work;
- how rewards are distributed.

---

### 7. Emission Must Be Dynamic but Limited

Nodo should not use unlimited emission.

Nodo also does not need a hard fixed maximum supply in the first design.

The preferred model is controlled dynamic emission.

The network may target an inflation rule, for example:

```text
target yearly inflation = 4%
```

The exact number can change later, but the principle is:

```text
each epoch has a maximum amount of new coins that can be created
```

A safe model is:

```text
NewEmissionCap = current circulating supply × target inflation rate / epochs per year
```

Then the epoch can use part of that cap depending on real network work.

Important rule:

```text
work can decide how much of the allowed cap is used
work must not create an unlimited cap
```

---

### 8. Fees Should Help Pay Security

Nodo should separate two sources of validator reward:

```text
1. fees already paid by users
2. new security emission allowed by policy
```

Recommended formula:

```text
RewardPool = FeesCollected + SecurityEmission
```

Where:

```text
SecurityEmission <= NewEmissionCap
```

This means:

- fees are recycled to validators;
- new coins are created only inside a controlled limit;
- more real network usage can help pay security;
- fake spam cannot create unlimited new supply.

---

### 9. Work Can Affect Emission, but Only Inside the Cap

Network work can affect how much new emission is used in an epoch.

Example:

```text
Emission cap for epoch: 100 NODO

Low useful work:
SecurityEmission = 30 NODO

Medium useful work:
SecurityEmission = 60 NODO

High useful work:
SecurityEmission = 100 NODO
```

But even if work is very high:

```text
SecurityEmission cannot exceed 100 NODO
```

This protects Nodo from inflation attacks.

---

### 10. Stake Helps Security, but Must Not Buy the Network

Validators should be able to lock coins to show commitment.

This is useful because a validator with locked coins has something to lose.

Recommended concept:

```text
StakeLock
```

A stake lock should record:

- validator address;
- amount locked;
- lock start;
- lock duration;
- unlock rules;
- slash or penalty rules;
- reward eligibility effect.

However, stake must not give unlimited power.

Bad model:

```text
2x stake = 2x control forever
```

Better model:

```text
more stake helps,
but each extra coin gives less additional influence
```

Possible rule:

```text
StakeWeight = sqrt(locked amount)
```

In simple terms:

```text
locking more helps,
but rich validators cannot buy unlimited dominance
```

---

### 11. Infrastructure Can Help, but Only Through Useful Work

Nodo should allow people to invest in validation capacity, similar to how miners invest in physical equipment, but the work must be useful to the network.

Useful infrastructure may include:

- reliable uptime;
- bandwidth;
- storage for historical data;
- ability to serve old blocks;
- fast validation;
- participation in integrity challenges;
- redundant node operation;
- availability proofs.

But Nodo should avoid a system where the richest hardware owner automatically dominates the network.

Correct direction:

```text
better infrastructure helps perform useful tasks
useful tasks can earn rewards
reward still follows epoch limits
```

---

### 12. Network Concentration Should Reduce Trust Weight

Multiple validators from the same network should not be banned automatically.

However, the network should treat concentration as a risk signal.

Possible rule:

```text
same IP or same network range = reduced trust factor
```

Recommended concept:

```text
NetworkClusterPenalty
```

This penalty should not say "you are guilty."

It should say:

```text
this validator appears less diverse,
so its trust weight is reduced
```

Examples:

```text
1 validator in a network: no penalty
2 validators in same network: light penalty
3 to 5 validators: medium penalty
6 or more validators: strong penalty
```

This can make attacks more expensive while avoiding unfair automatic bans.

---

### 13. Reward Distribution Should Use Work, Trust, and Risk

Nodo should calculate reward eligibility through valid work first.

A simple direction:

```text
ValidatorRewardShare =
ValidWorkWeight × TrustFactor × StakeFactor × NetworkDiversityFactor
```

Where:

- `ValidWorkWeight` comes from accepted work records;
- `TrustFactor` comes from validator score, 0 to 100;
- `StakeFactor` comes from locked coins with diminishing returns;
- `NetworkDiversityFactor` reduces concentrated validator clusters.

Then:

```text
ValidatorReward =
RewardPool × ValidatorRewardShare / TotalRewardShares
```

Important rule:

```text
RewardPool is limited before distribution starts.
```

This prevents infinite reward creation.

---

## Target Economic Flow

The target future flow should be:

```text
1. Chain starts with zero spendable supply.
2. Validators register identities.
3. Validators perform useful protection work.
4. Work records are written into blocks.
5. Validator scores are updated on-chain.
6. Epoch ends.
7. Fees collected during the epoch are counted.
8. New emission cap is calculated from policy.
9. Useful network work decides how much of the cap is used.
10. Reward pool is created from fees + allowed security emission.
11. Rewards are distributed to validators.
12. Each reward creates identifiable coin lots.
13. Future transactions must prove coin lot existence and ownership.
```

---

## Main On-Chain Records to Add

Nodo should gradually add these concepts:

```text
ValidatorIdentity
ValidatorScoreRecord
ValidationWorkRecord
ProtectionEpoch
EpochEmissionPolicy
GenesisRewardRecord
CoinLotRegistry
StakeLockRecord
NetworkClusterPenaltyRecord
RewardDistributionRecord
```

### ValidatorIdentity

Represents a validator known by the chain.

Should include:

- validator address;
- public key;
- registration epoch;
- current status;
- score;
- optional stake reference.

### ValidationWorkRecord

Records useful work performed by a validator.

Should include:

- validator address;
- epoch;
- work type;
- target object hash;
- result;
- proof or evidence hash;
- timestamp.

### ProtectionEpoch

Groups validation work and reward calculation.

Should include:

- epoch id;
- start block;
- end block;
- policy version;
- fees collected;
- emission cap;
- work factor;
- final reward pool.

### GenesisRewardRecord

Creates new coins as reward for protection work.

Should include:

- epoch id;
- validator address;
- reward amount;
- reward reason;
- work summary hash;
- policy version.

### StakeLockRecord

Records coins locked by a validator.

Should include:

- owner address;
- validator address;
- amount locked;
- start epoch;
- unlock epoch;
- penalty rules.

---

## Current Engine vs Target Protocol

Nodo already has many pieces that support the target direction:

```text
CoinLot
LedgerRecord
Block
Blockchain
State rebuild
Serialization codecs
Storage loader
SHA-256
SignatureProvider
Address
KeyPair
Audited provider boundary
Post-quantum provider interfaces
```

What needs to change over time:

```text
Current demo genesis mint
        ↓
zero-supply genesis + GenesisReward records

basic account names
        ↓
address-based validator and wallet identities

development signatures
        ↓
audited signature provider implementation

local blockchain engine
        ↓
networked validator protocol

simple chain validation
        ↓
validator consensus and epoch reward calculation
```

---

## What Nodo Is Not Yet

Nodo is not yet:

- a public decentralized network;
- a production cryptocurrency;
- a wallet system;
- a smart contract platform;
- a MetaMask-compatible chain;
- a production validator network;
- a production privacy system;
- a chain with audited real digital signatures fully connected;
- a system safe for real funds.

---

## Security Direction

Nodo should follow these security rules:

```text
Do not trust saved balances.
Do not trust disk files before validation.
Do not trust validator score unless it is derived from chain records.
Do not reward work unless the work is useful and verifiable.
Do not allow work to create unlimited emission.
Do not allow score to become automatic passive income.
Do not allow stake to buy unlimited network control.
Do not allow development signatures to pretend to be production signatures.
Do not allow privacy features to hide inflation.
```

---


---

## Implementation Start: Protection Economics Foundation

The first code step for the new economic model adds these foundations:

```text
ValidationWorkRecord
ValidatorScoreRecord
EpochEmissionPolicy
ProtectionEpoch
GenesisRewardRecord
```

This starts the migration from:

```text
demo genesis mint
```

toward:

```text
zero-supply chain + protection work + GenesisReward records
```

This does not remove the current demo mint yet. The old code remains while the new protection economics model is built and tested safely.

New test:

```text
tests/economics/ProtectionEconomicsTests.cpp
```

New test script:

```text
scripts/test_economics.sh
scripts/test_economics.bat
```

## Build

Nodo supports Linux-style shells and Windows builds.

### Fedora / Linux

```sh
sudo dnf install -y gcc gcc-c++ make
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

### Windows PowerShell / CMD

Requirements:

- MSYS2 UCRT64 or another GCC/G++ toolchain available in PATH;
- `gcc`;
- `g++` with C++20 support.

If using MSYS2 UCRT64, add this folder to PATH:

```text
C:\msys64\ucrt64\bin
```

Build and run:

```powershell
cmd /c scripts\clean.bat
cmd /c scripts\build.bat
.\build\nodo.exe
```

---

## Tests

### Linux / Fedora

Run all tests:

```sh
chmod +x scripts/*.sh
./scripts/test_all.sh
```

Run crypto tests only:

```sh
./scripts/test_crypto.sh
```

Run serialization tests only:

```sh
./scripts/test_serialization.sh
```

Run storage integration tests only:

```sh
./scripts/test_storage.sh
```

### Windows PowerShell / CMD

Run all tests:

```powershell
cmd /c scripts\test_all.bat
```

Run crypto tests only:

```powershell
cmd /c scripts\test_crypto.bat
```

Run serialization tests only:

```powershell
cmd /c scripts\test_serialization.bat
```

Run storage integration tests only:

```powershell
cmd /c scripts\test_storage.bat
```

---

## Expected Development Direction

This README replaces the old phase checklist with a concept-based roadmap.

The project should now evolve around these implementation tracks:

### Track A: Protection-Based Economics

Goal:

```text
replace arbitrary genesis supply with epoch-based GenesisReward records
```

Work items:

- define `ProtectionEpoch`;
- define `EpochEmissionPolicy`;
- define `GenesisRewardRecord`;
- make genesis start with zero spendable supply;
- create reward coin lots from epoch rewards;
- test controlled emission.

### Track B: Validator Trust and Work

Goal:

```text
reward useful protection work, not passive score
```

Work items:

- define `ValidatorIdentity`;
- define `ValidatorScoreRecord`;
- define score range from 0 to 100;
- define `ValidationWorkRecord`;
- separate score from reward;
- record validator work on-chain;
- use score as trust selection, not automatic income.

### Track C: Coin Existence and Traceability

Goal:

```text
every coin lot must be verifiably real before it can move
```

Work items:

- strengthen `CoinLot` identity;
- define `CoinLotRegistry`;
- validate lot existence before spending;
- reject already spent lots;
- connect reward lots to `GenesisRewardRecord`;
- preserve traceability through transfer and change lots.

### Track D: Stake and Infrastructure Security

Goal:

```text
allow validators to invest in security without buying unlimited control
```

Work items:

- define `StakeLockRecord`;
- add diminishing returns for stake influence;
- define infrastructure work types;
- reward useful availability and storage work;
- add uptime and challenge response records.

### Track E: Risk and Anti-Sybil Protection

Goal:

```text
make validator attacks more expensive and less profitable
```

Work items:

- define `NetworkClusterPenaltyRecord`;
- reduce trust factor for concentrated validators;
- detect same IP and same network range as risk signals;
- avoid automatic bans based only on network location;
- add gradual penalties;
- add conflict signing penalties.

### Track F: Production Cryptography

Goal:

```text
connect a real audited signature provider
```

Work items:

- choose audited Ed25519 or ECDSA provider;
- add official test vectors;
- document dependency and audit source;
- prevent development signatures from being used in production mode;
- preserve provider boundary for future algorithms.

### Track G: Network and Consensus

Goal:

```text
turn the local engine into a real validator network
```

Work items:

- peer discovery;
- node identity;
- mempool;
- validator selection;
- block proposal;
- block voting;
- finality rules;
- fork handling;
- slashing rules.

---

## Design Sentence

The long-term design sentence for Nodo is:

```text
Nodo is a security-first blockchain where coins are born from verified protection work, every coin lot has provable existence, validator trust is recorded on-chain, and emission is dynamically controlled by transparent economic rules.
```

---

## License

This project currently has no production-use license statement in this README.

Before public or commercial use, define the repository license clearly.
