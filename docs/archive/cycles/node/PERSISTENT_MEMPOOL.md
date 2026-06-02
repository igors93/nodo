> Archived document.
> This file is kept for historical context only and may not describe the current Nodo implementation.

# Nodo Persistent Mempool

Status: Cycle 9 Implementation  
Version: NODO-PERSISTENT-MEMPOOL-V1

## Purpose

This phase persists pending transactions so the local node can recover them
after restart.

## File Layout

```text
.nodo/mempool/tx_<transaction-id>.nodo
```

## File Contents

```text
transactionId
acceptedAt
publicKeyMaterial
transaction
```

The current implementation is development-network only. It restores development
signatures from persisted transaction payload and public key material.

## Safety Rules

```text
duplicate identical transaction -> idempotent
different content same id       -> rejected
invalid transaction             -> rejected
finalized transaction           -> removed from persistent mempool
```
