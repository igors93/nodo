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
