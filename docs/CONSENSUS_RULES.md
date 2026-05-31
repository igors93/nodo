# Consensus Rules

A validator may vote for a candidate block only when:

- the validator is active in the validator registry;
- the vote is structurally valid;
- the vote signature verifies under the configured crypto policy;
- the block passed state-transition validation;
- the state-transition preview accepted the block without partial mutation;
- sender balances and nonces are valid in the preview context;
- the preview produced a deterministic post-state root for the candidate block;
- the vote references the expected block height, block hash, previous hash and
  consensus round.

A quorum certificate is valid only when enough active validator weight approves
the same block under the configured quorum threshold. Duplicate votes,
unregistered validators, invalid validator signatures and votes for a different
block or round invalidate the certificate. Finality means the block has a valid
quorum certificate, has been accepted by the finalizer and has been persisted
with an auditable finalized record.

Current localnet signs user transactions with Ed25519 through OpenSSL and
validator votes/proposals with BLS12-381 through blst. Future testnet and
mainnet configs must refuse startup unless the configured crypto suite and key
store are production-safe for that network profile.
