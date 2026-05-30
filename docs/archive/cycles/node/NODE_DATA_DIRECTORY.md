# Nodo Persistent Node Data Directory

Status: Cycle 7 Implementation  
Version: NODO-NODE-DATA-DIR-V1

## Purpose

This phase adds a durable local node directory.

The directory stores enough information for an operator to inspect which network
and genesis the local node was initialized for.

## Files

```text
.nodo/
  manifest.nodo
  genesis.nodo
  blocks/
  mempool/
  peers/
    local_peer.nodo
  runtime/
    runtime_snapshot.nodo
```

## Manifest Fields

```text
chainId
networkName
protocolVersion
genesisConfigId
latestBlockHeight
latestBlockHash
validatorCount
peerCount
createdAt
updatedAt
```

## Security Rules

Initialization is safe and idempotent:

```text
same genesis id    -> already initialized
different genesis  -> rejected
invalid manifest   -> rejected
invalid peer       -> rejected
invalid genesis    -> rejected
```

The node never silently reuses a data directory for a different genesis.
