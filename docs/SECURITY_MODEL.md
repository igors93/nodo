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
files, storage schema files, mempool files and local key files through strict
codecs. Unknown fields, wrong versions, missing required fields, non-canonical
content and `.tmp` artifacts are not treated as canonical state.

`nodo chain audit` is a central local safety command. It reloads the runtime and
delegates protocol invariants to `ProtocolInvariantChecker` and operational
consistency checks to `ChainAuditor`. The audit covers manifest identity, chain
tip height/hash, `latestStateRoot`, finality bounds, finalized-block linkage,
crypto context, mempool validity and validator count consistency. It reports
failures instead of silently repairing suspicious state.

Before a block can receive votes, `StateTransitionEngine` applies transactions
against a temporary account-state view through an authoritative protocol context.
The engine refuses structural-only contexts, contexts that allow missing
accounts, contexts without chain-bound crypto authorization and contexts without
the canonical protocol-domain executor. A failing balance, nonce, fee,
authorization or payload check rejects the block and leaves the original runtime
state unchanged. Localnet uses explicit bootstrap-validator account allocations
in `GenesisConfig` only for development; they are documented as a limitation,
not a production supply model. Successful engine execution produces deterministic
state and receipts roots so later persistence, reload and audit checks can commit
to the resulting account state and protocol domains. Reload and artifact import
use a single canonical `ProtocolReplayState`; account-only replay is not accepted
as proof of a manifest `latestStateRoot`.

Finalized block reload verifies the quorum certificate and finalized record
before accepting the artifact: quorum threshold, duplicate validator votes,
unknown validators, invalid vote signatures and certificate/block mismatch all
reject reload. Slashing evidence and validator penalty decisions are now
separate auditable records: evidence must be verified first, and each evidence
id can produce only one deterministic penalty decision. Production stake
slashing remains outside the current localnet activation path.

Local key files are written atomically and parsed strictly. The current
localnet key format stores private material for the temporary provider and must
not be used for production networks.
