# Security Model

Nodo is defensive. The network must reject invalid data, record evidence,
reduce influence from bad actors and continue operating. It must never attack
back.

Misbehavior is protocol-relevant only when backed by verifiable evidence:

- invalid block evidence: a block fails structural or transition validation;
- invalid vote evidence: a vote targets an invalid block or malformed round;
- double-sign evidence: one validator signs conflicting votes for the same
  height and round;
- storage corruption evidence: canonical files fail strict parsing or hashes;
- peer quarantine evidence: repeated invalid protocol payloads from a peer.

Peers that are slow, offline or temporarily unreachable are not malicious by
default. Quarantine must be reversible or time-limited unless strong evidence
proves severe validator behavior. Severe penalties require auditable evidence.

The current implementation already rejects corrupted manifests, finalized block
files and mempool files through strict codecs. Additional evidence records and
penalty state should be implemented as small protocol types instead of ad hoc
runtime flags.
