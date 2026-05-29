# Nodo Coin Lot Transaction Integration

Status: Implementation Guide  
Version: NODO-COIN-LOT-TRANSACTION-INTEGRATION-V1

## Purpose

This document describes the phase where Nodo starts connecting transactions directly to the `CoinLotRegistry`.

In simple terms:

```text
a transaction cannot move coins only because a balance number looks correct
it must be backed by real spendable coin lots
```

## New Components

This phase adds:

```text
CoinLotTransactionValidationResult
CoinLotTransferPlan
CoinLotTransactionValidator
```

## What This Changes

Before this phase, transfer logic selected and spent coin lots directly inside `State`.

After this phase:

```text
Transaction
    ↓
CoinLotTransactionValidator
    ↓
CoinLotTransferPlan
    ↓
CoinLotRegistry
    ↓
State
```

This makes coin movement easier to test and safer to maintain.

## Security Rules

The validator checks:

```text
transaction is TRANSFER
sender and recipient are valid
amount is positive
fee is not negative
nonce is positive
registry is valid
sender has enough spendable coin lots
locked, spent and slashed lots are ignored
```

## Transfer Plan

A `CoinLotTransferPlan` records:

```text
which input lots are consumed
which output lots are created
how much goes to recipient
how much goes to fee pool
how much returns as change
```

The important rule is:

```text
total inputs = total outputs
```

That prevents hidden inflation inside transfers.

## State Integration

`State::applyTransferTransaction` now delegates to:

```text
State::applyTransferTransactionUsingRegistry
```

The old public method remains, so existing code continues to call the same function.

Internally, State now builds a registry view from its coin lots, applies the transfer through the registry, then replaces its coin lot view from the registry.

## What This Does Not Do Yet

This phase does not yet add transaction-declared input lot IDs.

For now, input selection is deterministic and automatic.

Future versions may allow a transaction to explicitly declare the exact input lots it wants to spend.

## New Test

```text
tests/core/CoinLotTransactionIntegrationTests.cpp
```

The test checks:

```text
deterministic transfer plan generation
registry-backed transfer application
locked/spent/slashed lots are rejected
State applies transfers through CoinLotRegistry
insufficient CoinLot balance is rejected without changing State
```
