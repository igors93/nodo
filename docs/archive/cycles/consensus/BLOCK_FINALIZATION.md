> Archived document.
> This file is kept for historical context only and may not describe the current Nodo implementation.

# Nodo Block Finalization

Status: Cycle 4 Implementation  
Version: NODO-BLOCK-FINALIZATION-V1

## Purpose

This phase adds local block finalization using a quorum certificate.

In simple terms:

```text
a block is not final just because it exists
a block becomes final only when enough validators precommit to it
```

## New Components

```text
FinalizedBlockRecord
BlockFinalizationRegistry
BlockFinalizationResult
BlockFinalizer
```

## Finalization Flow

```text
candidate block
        ↓
validator votes
        ↓
QuorumCertificate
        ↓
BlockFinalizer
        ↓
Blockchain append
        ↓
FinalizedBlockRecord
```

## Security Rules

A block can finalize only when:

```text
blockchain is not empty
blockchain is valid
block can append to the current tip
block is not genesis
quorum certificate verifies
certificate block hash matches block hash
certificate height matches block height
certificate previous hash matches block previous hash
finalization registry has no conflicting block at that height
```

## Conflict Rule

The registry allows this:

```text
same height + same block hash = duplicate finalization
```

It rejects this:

```text
same height + different block hash = conflicting finalization
```

## What This Does Not Do Yet

This is local finalization. It does not yet include P2P propagation, fork-choice
sync, or persistent finalized checkpoints.
