# Real Transport + Gossip Mesh

This phase adds the first transport and gossip boundary for Nodo.

It does not turn Nodo into a public mainnet node yet. It gives the project the
clean building blocks needed to move messages between nodes without mixing
transport, peer registry, validation, gossip routing and consensus rules in one
place.

## Added modules

- `p2p::Transport`: interface for message delivery between nodes.
- `p2p::TransportMessage`: one routed envelope from one node to another.
- `p2p::LoopbackTransport`: deterministic in-process transport for local network tests.
- `p2p::TransportFrameCodec`: canonical frame wrapper around `NetworkEnvelope` bytes.
- `p2p::PeerHandshakeManager`: validates first peer hello messages before trusting a peer.
- `p2p::GossipMesh`: broadcasts and receives gossip through known peers.
- `p2p::GossipInbox`: stores accepted inbound messages by type.

## What works now

A local test network can:

1. Register peer metadata.
2. Connect local nodes through the transport boundary.
3. Broadcast transaction, block, vote, certificate or status messages.
4. Flush outbound messages into transport.
5. Receive inbound transport messages.
6. Validate network id, chain id, protocol version, TTL, duplicate message id and rate limits.
7. Quarantine a peer after repeated invalid messages.

## Why loopback transport exists

The loopback transport is not fake consensus. It is a deterministic transport
implementation for local tests. It proves that the node can move real
`NetworkEnvelope` objects through the same transport interface that future TCP,
QUIC or libp2p implementations should use.

Keeping this interface clean avoids coupling consensus logic to one socket
library too early.

## Not included yet

- Production TCP server loop.
- Encrypted peer channels.
- NAT traversal.
- Persistent peer database.
- Full peer discovery.
- Bandwidth accounting.

Those should be implemented after this transport boundary is stable and tested.


## Hardened runtime path

The TCP testnet runtime now creates a hardened `GossipMesh`. Handshake messages are the only unauthenticated messages. After handshake, non-handshake envelopes must be protected by an authenticated encrypted peer session and then pass the inbound envelope validator. Rate limits are per peer and message type, invalid messages consume extra budget, and quarantine disconnects the peer. Peer registration is no longer only a registry write: the mesh applies `EclipseGuard` before admitting a new cryptographic peer identity.

## Discovery/reconnection policy

Static peers, persisted peers and peers learned from UDP discovery now converge on `PeerReconnectionPolicy`. Discovery is a source of candidates, not a direct socket-opening shortcut. The orchestrator seeds discovery, records candidates, attempts only the due candidates per tick, initiates the authenticated handshake after a successful provisional TCP connection, and records success/failure so backoff grows deterministically. Quarantined peers are not retried.


## Authenticated peer exchange

Peer exchange is no longer an internal helper only. `PEER_EXCHANGE` is a canonical network message delivered through the same authenticated gossip boundary as consensus and sync traffic. Received entries are capped, strictly decoded, checked against `EclipseGuard`, persisted as untrusted reconnect candidates and fed into `PeerReconnectionPolicy`; they are not registered as trusted peers until the normal signed handshake succeeds.
### Connection slot policy

The TCP testnet transport now treats connection capacity as a protocol admission policy. Pending handshakes remain capped by total/IP/subnet limits and token buckets, while authenticated connections are capped by total, inbound, outbound, per-IP and per-/24 subnet slots. When a total or directional slot is full, the oldest replaceable connection is evicted deterministically; when an IP or subnet is saturated, new peers are rejected instead of weakening diversity. This keeps discovery and peer exchange useful without allowing one address block to occupy the node.
