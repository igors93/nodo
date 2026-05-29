# Nodo GenesisReward Main State Flow

Status: Implementation Guide  
Version: NODO-GENESIS-REWARD-MAIN-STATE-FLOW-V1

## Purpose

This document describes the phase where `GenesisRewardRecord` becomes part of the main public `State` flow.

In simple terms:

```text
reward coins can now enter State directly through GenesisReward
not only through the old demo MintRecord path
```

## What Changed

This phase adds:

```text
State::applyGenesisRewardRecord
GenesisRewardRecord::deserialize
ChainStateRebuilder::rebuildStateFromGenesisRewardRecords
GENESIS_REWARD handling in ChainStateRebuilder::rebuildStateFromLedgerRecords
```

## Why This Matters

Before this phase, `GenesisRewardRecord` could exist as a ledger record and could create a CoinLot in isolated tests.

After this phase, the main state reconstruction path can process:

```text
GENESIS_REWARD ledger record
        ↓
GenesisRewardRecord
        ↓
reward CoinLot
        ↓
State total supply
        ↓
spendable validator balance
```

## Migration Rule

`MintRecord` still exists for compatibility with older demo code and tests.

But the production-oriented supply creation path is now:

```text
GenesisRewardRecord
```

The project should gradually move new supply creation away from:

```text
MintRecord
```

toward:

```text
GenesisRewardRecord
```

## Security Rules

This phase enforces:

```text
duplicate GenesisReward records are rejected
GenesisReward must be structurally valid
GenesisReward creates deterministic reward CoinLot
reward CoinLot origin must match GenesisReward deterministic id
State supply audit counts both legacy MintRecord and GenesisRewardRecord
ChainStateRebuilder accepts protection metadata as public-state no-op
```

## What This Does Not Do Yet

This phase does not yet create rewards automatically at epoch end.

That future phase should add:

```text
RewardDistributor
```

which will calculate:

```text
accepted work
validator score
emission cap
reward share
GenesisReward records
```

## New Test

```text
tests/core/GenesisRewardStateFlowTests.cpp
```
