# Networked Consensus Foundation

This package adds the first clean network-consensus boundary for Nodo. It does
not pretend to be a production P2P stack yet. The goal is to prepare the node for
multi-peer operation without mixing sockets, gossip, consensus and storage in one
place.

## Added modules

- `p2p::NetworkEnvelope`: canonical message envelope with network id, chain id,
  protocol version, message type, sender id, TTL, payload hash and deterministic
  message id.
- `p2p::InboundMessageValidator`: local policy gate for inbound messages. It
  rejects wrong network, wrong chain, wrong protocol version, expired messages,
  duplicate message ids and basic rate-limit violations.
- `p2p::PeerRegistry`: local peer metadata registry with quarantine support.
- `p2p::OutboundMessageQueue`: per-peer outbound queue with capacity limits.
- `node::ChainStatusMessage`, `node::BlockLocator`, `node::NetworkBlockSyncRequest`:
  first chain-sync primitives.
- `consensus::VotePool`: deduplicates validator votes by validator, height and
  round, and captures conflicting votes as future slashing evidence input.
- `consensus::QuorumAssembly`: bridge from `VotePool` into the existing
  `QuorumCertificateBuilder`.

## Why this matters

Nodo already has local finalization and state validation. The next protocol risk
is accepting unstructured network messages directly into consensus. This package
creates the boundary where messages must be identified, scoped to one chain,
validated for freshness, deduplicated and only then passed to consensus modules.

## Not included yet

- Real TCP/QUIC/libp2p transport.
- Encrypted peer channels.
- Full gossip mesh.
- Slashing penalties.
- Binary canonical message codec.

Those should be implemented after this boundary is merged and stable.


## Required P2P gate for consensus traffic

Consensus traffic reaches `ConsensusEventLoop` only after the P2P admission gate accepts it. The gate requires authenticated sessions for `BLOCK_PROPOSAL`, `VALIDATOR_VOTE`, QC, sync and slashing messages; validates `NetworkEnvelope` fields and payload hash; applies per-message-type rate limits; and records/quarantines abusive peers. This keeps consensus code free of transport-specific checks while preventing unauthenticated or malformed network traffic from entering the protocol path.
