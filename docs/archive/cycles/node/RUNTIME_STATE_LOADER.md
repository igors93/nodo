# Nodo Runtime State Loader

Status: Cycle 9 Implementation  
Version: NODO-RUNTIME-STATE-LOADER-V1

## Purpose

This phase lets Nodo rebuild a local runtime from a persistent node data
directory.

## What It Loads

```text
manifest.nodo
blocks/block_<height>.nodo
mempool/tx_<transaction-id>.nodo
```

## Flow

```text
data directory
        ↓
manifest validation
        ↓
genesis runtime rebuild
        ↓
finalized block files appended in order
        ↓
persistent mempool loaded
        ↓
runtime audit
```

## Security Rules

The loader rejects:

```text
invalid directory config
missing manifest
genesis id mismatch
missing block file
invalid block file
block that cannot append to current tip
rebuilt latest hash not matching manifest
invalid persistent mempool file
```

This is the first step toward restarting a node and continuing from local disk.
