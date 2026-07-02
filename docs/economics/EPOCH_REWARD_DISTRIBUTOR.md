# Nodo Epoch Reward Distributor

Status: Implementation Guide  
Version: NODO-EPOCH-REWARD-DISTRIBUTOR-V1

## Purpose

This document describes the first reward distributor for Nodo protection epochs.

In simple terms:

```text
accepted validator work
+ validator score
+ emission policy
+ epoch demand
        ↓
GenesisReward records
```

## What This Adds

This phase adds:

```text
EpochRewardDistributor
EpochRewardDistribution
ValidatorRewardShare
```

## Core Rule

The distributor mints only the epoch `securityEmission`.

It does **not** mint `feesCollected`.

That is important because fees are existing coins. Minting fees again would create hidden inflation.

```text
securityEmission = new coins
feesCollected = existing coins
rewardPool = securityEmission + feesCollected
GenesisReward = securityEmission only
```

## Reward Formula

The current first version uses:

```text
accepted work weight × validator trust factor
```

Where:

```text
score 0   -> 0 bps
score 50  -> 5000 bps
score 100 -> 10000 bps
```

A validator with accepted work but no score does not receive minted rewards.

This is conservative and helps avoid unknown validators receiving new supply without an on-chain trust record.

## Work Demand

The epoch does not always use the whole emission cap.

It uses:

```text
workDemandBasisPoints = accepted work / target work
```

Limited to:

```text
10000 bps = 100%
```

So if the network only performs half of the target work, only half of the emission cap is used.

## Security Rules

This phase rejects:

```text
invalid work records
invalid score records
duplicate accepted evidence
zero target work
invalid epoch ranges
negative supply
negative fees
empty accepted block hash
zero or negative timestamp
```

It also enforces:

```text
distributed security emission <= epoch security emission
sum of rewards == distributed security emission
reward records are deterministic
reward ids are unique
fees are not minted
```

## Why Duplicate Evidence Is Rejected

Duplicate evidence could accidentally or maliciously count the same work twice.

The distributor treats duplicate accepted evidence as an error instead of quietly rewarding it.

## Follow-up: Automatic Epoch Settlement

A later phase wired automatic reward insertion into the block pipeline:
`node::EpochRewardSettlementService` builds candidate reward records at epoch
settlement heights during block production (`BlockProductionPhase`), and
`RuntimeBlockPipeline` rejects any finalized block whose epoch reward records
do not match the deterministic settlement
(`EpochRewardSettlementService::candidateRecordsMatch`).

## New Test

```text
tests/economics/EpochRewardDistributorTests.cpp
```
