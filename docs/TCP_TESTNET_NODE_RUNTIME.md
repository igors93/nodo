# TCP Testnet Node Runtime

This phase adds the first socket-backed testnet runtime boundary for Nodo.

It does not claim that Nodo is ready for a public mainnet. It gives the project a
clean way to run multiple local nodes over TCP while preserving the separation
created by the previous phases:

- protocol messages stay inside `NetworkEnvelope`;
- gossip continues through `GossipMesh`;
- delivery happens through the `Transport` interface;
- TCP framing is canonical and size-limited;
- peer metadata can be loaded from and saved to a local `peers.conf` file.

## Added modules

- `p2p::TcpTransportFrameCodec`
- `p2p::TcpTransport`
- `node::TcpTestnetNodeRuntimeConfig`
- `node::TcpTestnetPeerFileEntry`
- `node::TcpTestnetPeerStore`
- `node::TcpTestnetNodeRuntime`

## What works now

A local testnet runtime can:

1. Bind a node to a TCP port.
2. Register peer endpoints.
3. Connect to another local node.
4. Send canonical transport frames over TCP.
5. Poll inbound TCP frames without background threads.
6. Feed accepted messages into the existing gossip mesh.
7. Persist and reload a simple peer file.

## Why this is synchronous

This runtime intentionally does not start threads. Operators and tests call
`tick(now)` to flush outbound messages and receive inbound messages.

That makes behavior deterministic, easier to test on low-power machines and safer
while the protocol is still evolving.

## Peer file format

`peers.conf` uses one peer per line:

```text
nodeId host port publicKeyFingerprint
```

Example:

```text
node-a 127.0.0.1 30333 fingerprint-a
node-b 127.0.0.1 30334 fingerprint-b
```

## Implemented since this boundary

- encrypted peer channels gated by `EncryptedPeerTransport`
  (`ENCRYPTED_PEER_CHANNELS.md`);
- Ed25519 challenge authentication during connection setup
  (`PeerHandshakeManager`);
- persistent block sync over TCP (`PERSISTENT_BLOCK_STATE_SYNC.md`);
- automatic peer discovery and reconnection policy (`DiscoveryService`,
  `PeerReconnectionPolicy`).

## Not included yet

- NAT traversal;
- bandwidth accounting;
- production-grade async event loop.

## Bootstrap and reconnection

The TCP runtime remains a deterministic tick-driven transport, but the daemon path now treats peer connectivity as policy. `NodeDaemon` registers `--peer` entries as bootstrap candidates; `NodeOrchestrator` feeds them into discovery and `PeerReconnectionPolicy`; and only due candidates are connected. A disconnected peer is retried after backoff instead of every tick, and an authenticated session clears the pending reconnect state.
### Connection slot policy

The TCP testnet transport now treats connection capacity as a protocol admission policy. Pending handshakes remain capped by total/IP/subnet limits and token buckets, while authenticated connections are capped by total, inbound, outbound, per-IP and per-/24 subnet slots. When a total or directional slot is full, the oldest replaceable connection is evicted deterministically; when an IP or subnet is saturated, new peers are rejected instead of weakening diversity. This keeps discovery and peer exchange useful without allowing one address block to occupy the node.


## Peer penalty persistence

`peers.conf` persists reputation state for authenticated peers: score, quarantine flag, invalid-message count, temporary ban expiration and canonical ban reason. Loading peers restores active bans and drops expired ones through the same runtime lifting path used during ticks.
