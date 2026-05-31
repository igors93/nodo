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

`nodo chain audit` is a central local safety command. It reloads the runtime and
delegates consistency checks to `ChainAuditor`, which verifies manifest identity,
chain tip height/hash, crypto context, mempool validity and validator count
consistency. It reports failures instead of silently repairing suspicious state.

Local key files are written atomically and parsed strictly. The current
localnet key format stores private material for the temporary provider and must
not be used for production networks.
