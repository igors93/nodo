> Archived document.
> This file is kept for historical context only and may not describe the current Nodo implementation.

# Nodo Runtime Block Pipeline

Status: Cycle 8 Implementation  
Version: NODO-RUNTIME-BLOCK-PIPELINE-V1

## Purpose

This phase connects the runtime to local block production and finalization.

The pipeline uses components already implemented in previous cycles:

```text
Mempool
MempoolBlockProducer
ValidatorVoteRecord
QuorumCertificateBuilder
BlockFinalizer
```

## Flow

```text
runtime mempool
        ↓
candidate block production
        ↓
development validator votes
        ↓
quorum certificate
        ↓
block finalization
        ↓
finalized transactions removed from mempool
```

## Security Rules

The pipeline rejects:

```text
invalid runtime
invalid config
empty mempool
invalid mempool
not enough validators
quorum build failure
finalization failure
```

## Important Limitation

This phase uses development-only local votes. Real remote validator voting and
persistent validator key storage come later.
