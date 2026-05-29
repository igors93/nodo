# Nodo Explicit Transaction CoinLot Inputs

Status: Implementation Guide  
Version: NODO-EXPLICIT-TRANSACTION-COIN-LOT-INPUTS-V1

## Purpose

This document describes the phase where Nodo transactions can declare the exact `CoinLot` ids they want to spend.

In simple terms:

```text
the transaction now says which coin lots it wants to use
validators must spend only those declared lots
validators must not silently replace them with other lots
```

## Why This Matters

Before this phase, the node could select spendable lots automatically.

That is useful for a local prototype, but a real blockchain needs transaction intent to be explicit and auditable.

Explicit inputs improve security against:

```text
double-spend confusion
validator input substitution
transaction ambiguity
transaction-id mismatch between nodes
hidden changes in spend intent
```

## New Transaction Behavior

A transfer can now be created with explicit input lots:

```text
Transaction(
    TRANSFER,
    from,
    to,
    amount,
    fee,
    nonce,
    timestamp,
    {"lot-a", "lot-b"}
)
```

The transaction id commits to these input ids through the signing payload.

Example:

```text
inputLots=[lot-a,lot-b]
```

Changing the input list changes the transaction id.

## Backward Compatibility

Legacy/demo transactions still work.

If a transaction does not declare input lots, Nodo keeps the old deterministic automatic selection mode.

Important compatibility rule:

```text
inputLots=[] is not added to legacy signing payloads
```

This avoids changing older transaction ids.

## Validator Rule

If explicit input lots are present:

```text
only those lots may be used
all declared lots must exist
all declared lots must belong to the sender
all declared lots must be spendable
declared lots must cover amount + fee
duplicates are rejected
unsafe ids are rejected
```

If the declared lots are not enough, the transaction is rejected even if the sender owns other available lots.

This prevents silent input substitution.

## New Test

```text
tests/core/TransactionExplicitInputTests.cpp
```

The test checks:

```text
input lots affect transaction id
serialization/deserialization preserves input lots
legacy payload remains compatible
explicit inputs are used exactly
silent input substitution is rejected
missing/wrong-owner/duplicate/unsafe inputs are rejected
automatic legacy selection still works
```
