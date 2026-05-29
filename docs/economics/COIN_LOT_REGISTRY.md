# Nodo Coin Lot Registry

Status: Implementation Guide  
Version: NODO-COIN-LOT-REGISTRY-V1

## Purpose

This document describes the first official coin lot existence registry in Nodo.

In simple terms:

```text
before a coin can move,
Nodo must prove that the coin lot exists,
belongs to the sender,
is available,
and was not already spent.
```

## New Components

This phase adds:

```text
CoinLotVerificationResult
CoinLotRegistry
CoinLotRegistryRebuilder
```

## Why This Matters

Nodo is not only a balance table.

Nodo tracks coin lots.

A coin lot has:

```text
id
origin
owner
amount
status
history
```

The registry answers:

```text
does this lot exist?
is it spendable?
who owns it?
what amount does it contain?
is it locked, spent, or slashed?
```

## Current Scope

This first version rebuilds reward coin lots born from `GenesisRewardRecord`.

That means:

```text
GenesisReward
        ↓
reward CoinLot
        ↓
CoinLotRegistry
```

## Spend Safety

The registry can consume one input lot and create output lots.

Rule:

```text
sum(outputs) must equal input amount
```

Example:

```text
input: 50 NODO

outputs:
20 NODO to receiver
29 NODO as change
1 NODO fee

total outputs = 50 NODO
```

The input becomes spent and cannot be spent again.

## What This Does Not Do Yet

This phase does not yet fully replace the old transaction engine.

It prepares the safe registry needed for that migration.

Future phases should connect:

```text
Transaction
State
CoinLotRegistry
LedgerRecord
Block validation
```

## Security Rule

A coin lot registry is not independent truth.

It must be rebuildable from accepted blockchain history.
