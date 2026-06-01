# Canonical Serialization + State Commitment

This phase introduces the first strict byte-level canonical encoding boundary for Nodo.
It does not remove existing human-readable `serialize()` methods. Those remain useful
for diagnostics. Critical protocol data can now be encoded into deterministic bytes,
hashed with explicit domains, and audited through explicit state commitments.

## Added boundaries

- `serialization::CanonicalWriter`
- `serialization::CanonicalReader`
- `serialization::CanonicalHash`
- `serialization::ProtocolMessageCodec`
- `serialization::ConsensusCanonicalCodec`
- `core::StateCommitment`
- `node::StateSnapshot`
- `node::StateReplayAuditor`

## Security rules

Canonical encoding uses fixed-endian integers, length-prefixed strings and bytes,
strict size limits, and full-consumption checks. A decoded payload with trailing
bytes is rejected by codec boundaries.

State commitments are calculated from:

- sorted account state;
- ordered ledger records;
- validator registry serialization;
- finalized block height and hash.

The final state root is domain-separated from the component roots so a root from
one context cannot be silently reused in another context.

## What this does not do yet

- It does not replace every existing text serialization call.
- It does not implement full historical chain replay from disk.
- It does not add Merkle proofs yet.
- It does not implement a binary block storage migration yet.

Those should come after this canonical boundary is stable.
