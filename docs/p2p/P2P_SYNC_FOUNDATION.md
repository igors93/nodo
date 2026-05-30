# Nodo P2P Message and Local Sync Foundation

Status: Cycle 5 Implementation  
Version: NODO-P2P-SYNC-V1

## Purpose

This phase adds the first P2P protocol objects without opening sockets yet.

The goal is to define safe message envelopes and local sync planning.

## New Components

```text
PeerInfo
PeerMessage
PeerMessageFactory
LocalSyncPlan
LocalNodeSynchronizer
```

## Message Types

```text
HANDSHAKE
CHAIN_SUMMARY
BLOCK_ANNOUNCEMENT
BLOCK_REQUEST
BLOCK_RESPONSE
TRANSACTION_BROADCAST
VOTE_BROADCAST
QUORUM_CERTIFICATE_BROADCAST
SYNC_REQUEST
SYNC_RESPONSE
ERROR
```

## Local Sync Flow

```text
local chain summary
        ↓
remote chain summary
        ↓
finality-safe comparison
        ↓
request blocks or reject peer
```

## Security Rules

Nodo rejects:

```text
invalid peer metadata
invalid remote chain summary
remote chain behind local finality
remote finality conflicting with local finality
remote chain that is not better
```

This is not full networking yet. It is the protocol and decision foundation for
future socket transport.
