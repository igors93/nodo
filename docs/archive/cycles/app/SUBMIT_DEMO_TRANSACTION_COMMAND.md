# Nodo submit-demo-transaction Command

Status: Cycle 9 Implementation  
Version: NODO-CLI-SUBMIT-DEMO-TRANSACTION-V1

## Purpose

This command writes a pending development transaction into the persistent
mempool.

```bash
nodo submit-demo-transaction --data-dir .nodo
```

Then a block can be produced from the persistent mempool:

```bash
nodo produce-demo-block --data-dir .nodo
```

## Operational Flow

```text
init node
        ↓
submit demo transaction
        ↓
transaction is written under .nodo/mempool/
        ↓
produce demo block
        ↓
transaction is finalized and removed from persistent mempool
```
