# Consensus Rules

A validator may vote for a candidate block only when:

- the validator is active in the validator registry;
- the vote is structurally valid;
- the vote signature verifies under the configured crypto policy;
- the validation context is authoritative before any vote is cast: chain
  identifier configured, crypto context valid, transaction signatures
  verifiable, account state enforced, missing-account fallback disabled and
  canonical protocol-domain executor available; proposals received before this
  condition is met are skipped, not accepted;
- the block passed state-transition validation;
- the authoritative state-transition engine accepted the block without partial
  mutation;
- sender balances and nonces are valid in the execution context;
- when coin lot preview is enabled, every TRANSFER transaction is backed by
  spendable lots and no double-spend occurs within the block;
- execution produced deterministic post-state and receipts roots for the candidate
  block, including the coin lot registry digest when coin lot preview is active;
- the vote references the expected block height, block hash, previous hash and
  consensus round.

A quorum certificate is valid only when enough active validator weight
PRECOMMITs to the same block under the configured quorum threshold. PREVOTE,
REJECT, UNKNOWN and legacy approval-style decisions cannot build a QC. Duplicate
votes, unregistered validators, invalid validator signatures and votes for a
different block or round invalidate the certificate. Finality means the block
has a valid PRECOMMIT quorum certificate, has been accepted by the finalizer and
has been persisted with an auditable finalized record.

Current localnet signs user transactions with Ed25519 through OpenSSL and
validator votes/proposals with BLS12-381 through blst. Future testnet and
mainnet configs must refuse startup unless the configured crypto suite and key
store are production-safe for that network profile.

## P2P Consensus Messages

The gossip layer carries the following consensus-specific message types:

- `BLOCK_PROPOSAL`: a proposer-signed candidate block broadcast before voting.
  Applied via `BlockAnnounceHandler`; the `ConsensusEventLoop` drives prevote
  and precommit rounds after validation.
- `VALIDATOR_VOTE`: a BLS12-381-signed vote (prevote or precommit) from an
  active validator. `ConsensusEventLoop` accumulates votes and builds a
  `QuorumCertificate` from PRECOMMIT votes when the 2/3 weight threshold is crossed. Duplicate votes
  and votes from unregistered validators are discarded.
- `QUORUM_CERTIFICATE`: an assembled QC broadcast after quorum is reached.
  Peers that missed votes may use this to advance their consensus state.
- `FINALIZED_BLOCK_ARTIFACT`: a `FinalizedBlockRecord` carrying the block index,
  block hash, previous hash, round, finalization timestamp and quorum
  certificate. Receiving nodes verify the QC against their local validator
  registry before recording finality. Malformed or unverifiable artifacts are
  silently discarded; no implicit trust is granted to peers.
- `BLOCK_SYNC_REQUEST` / `BLOCK_SYNC_RESPONSE`: used to fetch blocks a peer
  missed during downtime or initial block download.

All consensus message signatures are verified locally; no message is acted on
without signature verification against the validator registry.
