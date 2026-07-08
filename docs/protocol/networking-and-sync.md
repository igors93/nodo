# Networking and Sync

Nodo includes TCP transport, gossip, peer policy, and sync foundations. Networking is part of the security boundary because peers may send malformed, duplicated, delayed, or malicious data.

## Network components

- peer identity;
- static peers;
- TCP transport;
- gossip relay;
- peer exchange;
- peer authentication;
- rate limiting;
- temporary bans and quarantine;
- reconnect policy;
- eclipse-resistance foundations;
- block and state sync;
- JSON-RPC public API.

## Peer admission

A node should only admit peers that pass protocol-version, identity, network, and safety checks. Peer exchange should be bounded and authenticated to avoid untrusted peers filling all connection slots.

## Gossip

Gossip should avoid accepting or relaying invalid protocol data. The relay path must not bypass normal transaction, block, or vote validation.

## Sync

Sync should verify downloaded artifacts before accepting them. Fast sync and snapshot sync must still be tied to trusted finality evidence and canonical state commitments.

## Restart safety

Consensus recovery, QC persistence, finalized block records, and manifest validation should allow a node to restart without accepting divergent history.

## Public API

External integrations should use JSON-RPC at `POST /rpc`. Operational REST endpoints may remain for health, metrics, diagnostics, and local tests.
