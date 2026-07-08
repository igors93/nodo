# Blocks and Finalization

A block should become part of canonical history only after it passes validation and receives sufficient finality evidence.

## Block lifecycle

```text
transaction admission
      ↓
mempool selection
      ↓
block proposal
      ↓
state-transition preview
      ↓
validator voting
      ↓
PRECOMMIT quorum certificate
      ↓
finalized block artifact
      ↓
persistence and manifest update
      ↓
reload/audit verification
```

## Validation before voting

Validators should not vote for a block unless the block passes deterministic checks, including:

- previous block reference;
- transaction signature and nonce rules;
- fee/minimum-fee policy;
- state-transition execution;
- coin-lot ownership validation;
- protocol-domain record validation;
- resulting state root and receipt root consistency.

## Finalized artifacts

A finalized artifact must be sufficient for replay and audit. It should include consensus evidence and all protocol-domain records that affect state.

## QC persistence

Quorum certificates should be persisted so that restart and fast sync can verify finality without relying on volatile memory.

## Rejection principle

A node should reject a finalized artifact if its state transition, consensus evidence, canonical serialization, or protocol-domain records cannot be verified.
