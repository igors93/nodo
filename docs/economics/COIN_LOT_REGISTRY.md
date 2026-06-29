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

## Block Preview Integration

The `CoinLotRegistry` is now connected to block validation through
`StateTransitionPreview`. When a `StateTransitionPreviewContext` is configured
with `enableCoinLotPreview`, a working copy of the registry is applied per
transaction during preview:

```text
Block preview
    ↓
CoinLotTransactionValidator::applyTransfer (per TRANSFER transaction)
    ↓
working CoinLotRegistry (mutable copy, not canonical state)
    ↓
INVALID_TRANSACTION if lot ownership, balance, or status check fails
```

After all transactions are processed, the final registry state is serialized,
hashed, and inserted into the `stateRoot` computation under the `coin_lots`
domain. This means divergent registry state produces a different state root and
causes block rejection in `BlockValidationMode::ProtocolCommitment`.

## What Is Still Pending

- Transaction-declared explicit input lot IDs (currently selection is automatic
  and deterministic).
- Coin lot registry persistence to disk and full rebuild-from-history path for
  multi-block sync scenarios.
- Slashing-lot lifecycle integration (slash deduction from locked lots).

## Security Rule

A coin lot registry is not independent truth.

It must be rebuildable from accepted blockchain history.
