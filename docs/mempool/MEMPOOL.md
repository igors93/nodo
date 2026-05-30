# Nodo Mempool

Status: Cycle 3 Implementation  
Version: NODO-MEMPOOL-V1

## Purpose

This phase adds the first transaction admission queue.

The mempool stores valid transactions before they are selected for a block.

## New Components

```text
MempoolConfig
MempoolEntry
MempoolAdmissionResult
Mempool
```

## Admission Rules

A transaction is admitted only when:

```text
mempool config is valid
transaction is structurally valid under crypto policy
transaction has enough fee
transaction id is not already present
sender+nonce is not already present unless replacement is allowed
mempool capacity is not exceeded
```

## Replacement Rule

If replacement is enabled, a transaction with the same sender+nonce may replace
an older one only when it pays a strictly higher fee.

## Block Selection

Transactions selected for a block are ordered by:

```text
higher fee first
older acceptedAt first
transaction id as final deterministic tie-breaker
```

## What This Does Not Do Yet

This mempool does not mutate State and does not decide final validity against
balances. Future block production must still run full state-transition
validation before accepting transactions into a block.
