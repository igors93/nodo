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

### Consensus relay

Validators are not assumed to form a full mesh. Quorum needs more than two
thirds of the voting weight ([ADR 0002](../spec/adr-0002-fault-model-and-quorum.md)),
so a validator that only heard its direct peers could stall even with every
validator online. `ConsensusEventLoop` therefore forwards consensus messages:

- a vote is relayed, as `VALIDATOR_VOTE`, only when the local vote pool newly
  accepts it: a valid signature from an eligible validator, for the current
  height and the current or next round. A copy of a known vote is a duplicate,
  so each node forwards each vote at most once and cycles stop. Stale,
  invalid and conflicting votes are never forwarded; a double vote travels as
  slashing evidence;
- a block proposal is relayed once, when it passes verification and becomes
  the candidate for the current round;
- a relayed message goes to every peer except the one that delivered it, and
  a node never relays its own loopback proposal.

A relaying peer forwards one copy of every validator's votes, so the per-peer
`VALIDATOR_VOTE` receive window is the network's
`maxGossipMessagesPerPeerWindow` multiplied by the validator-set size. Other
message types keep the single budget.

## Sync

Sync should verify downloaded artifacts before accepting them. Fast sync and snapshot sync must still be tied to trusted finality evidence and canonical state commitments.

## Restart safety

Consensus recovery, QC persistence, finalized block records, and manifest validation should allow a node to restart without accepting divergent history.

## Public API

External integrations should use JSON-RPC at `POST /rpc`. Operational REST endpoints may remain for health, metrics, diagnostics, and local tests.
