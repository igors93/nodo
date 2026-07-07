# Sync, finality and multi-node stabilization

This implementation hardens the finalized-block catch-up path used when a node
observes a peer-finalized height before it has the corresponding canonical
block locally.

## Problem

A node can receive a `FINALIZED_BLOCK_ARTIFACT` for height `H` while its local
chain is still at height `H - 1`. The correct behaviour is not to reject the
peer forever and not to wait for another proposal; the node must request the
missing finalized block range, verify the QC and canonical runtime transition,
persist the result, and converge to the same finalized state.

Before this stabilization, several sync decisions in `NodeOrchestrator` were
silent `continue` branches. That made multi-node CI failures difficult to
understand: an invalid request, a forked ancestor, a stale response, an
unauthenticated peer, or a full outbound queue all looked like “sync did not
finish”.

## New boundary

`SyncRecoveryPolicy` is the small policy module that classifies sync traffic
before the expensive import path runs:

- `ACCEPTED`: the request/response is safe to serve or import.
- `STALE`: the message is well-formed but no longer useful; do not count it as
  a protocol failure.
- `REJECTED`: the message is malformed, conflicting, not from the claimed peer,
  or does not connect to the canonical chain. Record it in `SyncHealth`.

The orchestrator now uses this policy for both `BLOCK_SYNC_REQUEST` and
`BLOCK_SYNC_RESPONSE`, and records explicit diagnostics when a critical sync
message cannot be queued.

## Safety rules

A sync request is served only when:

- the requester identity matches the envelope sender;
- the requested `fromHeight` exists on the responder;
- the requester supplied the responder's canonical ancestor hash for
  `fromHeight - 1`;
- the response batch size is bounded by the protocol batch limit.

A sync response is accepted for import only when:

- the source peer matches the envelope sender;
- the local sync checkpoint matches the canonical tip;
- any overlapping heights match the local chain exactly;
- the first new block connects directly to the local tip.

The full `PersistentBlockStateSyncApplier` still performs the expensive QC,
state-transition, slashing and persistence checks. `SyncRecoveryPolicy` is only
an early, explicit classification layer.

## Multi-node benefit

When governance/finality advances on one node while another node is one block
behind, the behind node now has a clearer and stricter recovery path:

1. finalized artifact for an unavailable block records a sync-watchdog target;
2. the watchdog requests the missing finalized block range;
3. the responder serves only canonical ranges connected to the requester's
   ancestor;
4. stale messages are ignored without poisoning sync health;
5. conflicting messages produce actionable diagnostics instead of silent stalls.
