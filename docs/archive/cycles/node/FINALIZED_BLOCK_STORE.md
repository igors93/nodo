# Nodo Finalized Block Store

Status: Cycle 8 Implementation  
Version: NODO-FINALIZED-BLOCK-STORE-V1

## Purpose

This phase persists finalized block artifacts and updates the runtime manifest.

## Block File

A finalized block is written to:

```text
.nodo/blocks/block_<height>.nodo
```

The file contains:

```text
blockIndex
blockHash
previousHash
transactionCount
serialized block
quorum certificate
finalized record
```

## Write Order

```text
1. write finalized block file
2. update runtime snapshot
3. update manifest
```

This prevents the manifest from claiming a new height before the block artifact
exists.

## Safety Rules

```text
same finalized block file  -> idempotent
different block same height -> rejected
missing manifest            -> rejected
different genesis           -> rejected
invalid runtime             -> rejected
```
