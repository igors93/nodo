# Nodo Protection Economics Implementation Start

Status: Implementation Guide  
Version: NODO-PROTECTION-ECONOMICS-IMPLEMENTATION-START-V1

## Purpose

This document marks the first code implementation step for Nodo's protection-based economic model.

The goal of this step is not to replace the old demo genesis mint immediately.

The goal is to add the first safe code boundaries for:

```text
validator work
validator score
epoch emission cap
protection epoch reward pool
GenesisReward coin creation
```

## New Code Components

This implementation adds:

```text
ValidationWorkRecord
ValidatorScoreRecord
EpochEmissionPolicy
ProtectionEpoch
GenesisRewardRecord
```

## What Changes Now

This step starts moving the project from:

```text
coins created by demo genesis mint
```

toward:

```text
coins created by accepted GenesisReward records after useful protection work
```

## What Does Not Change Yet

This step does not yet remove:

```text
MintRecord
current demo scenario
current transaction flow
current storage flow
```

Those remain so the project keeps building and testing.

The migration should happen gradually.

## Why This First Step Is Safe

This step only adds foundations and tests.

It does not yet rewrite consensus, networking, mempool, wallet, or state transition rules.

## New Test

The new test is:

```text
tests/economics/ProtectionEconomicsTests.cpp
```

It checks that:

- zero-supply bootstrap cap works;
- 4% example emission cap is calculated;
- protection epoch reward pool is capped;
- accepted work contributes to rewards;
- rejected work does not contribute to rewards;
- validator score remains bounded from 0 to 100;
- GenesisReward creates a traceable CoinLot.
