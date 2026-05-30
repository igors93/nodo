# Nodo Mempool Block Producer

Status: Cycle 4 Implementation  
Version: NODO-MEMPOOL-BLOCK-PRODUCER-V1

## Purpose

This phase adds a deterministic pipeline that turns admitted mempool
transactions into a candidate block.

## New Components

```text
BlockProductionConfig
BlockProductionPlan
BlockProductionResult
MempoolBlockProducer
```

## Production Flow

```text
Mempool
        ↓
fee ordered transaction selection
        ↓
LedgerRecord::fromTransaction
        ↓
BlockProductionPlan
        ↓
candidate Block
```

## Security Rules

The producer rejects:

```text
invalid block production config
empty or invalid blockchain
invalid mempool
empty mempool
not enough transactions
transaction that fails policy validation
candidate block that cannot append to current chain tip
```

## Important Design Rule

Producing a block does not mutate the mempool.

Transactions should be removed from the mempool only after the block is
finalized and accepted by the chain.
