# Nodo Protocol

Nodo Protocol is the deterministic protocol path used by localnet today and by
future testnet and mainnet networks by configuration. It is an experimental
Proof-of-Protection blockchain foundation, not a production mainnet.

The protocol path is:

1. build a candidate block from admitted transactions;
2. validate block structure;
3. validate signatures through the configured provider;
4. validate the state transition without partial mutation;
5. collect validator votes;
6. build a quorum certificate;
7. finalize the block;
8. persist the finalized artifact and manifest;
9. audit the chain during reload.

Deterministic protocol inputs include network parameters, genesis config,
transaction payloads, ledger records, block headers, quorum certificates,
finalized records and storage codecs. Nodes must reject ambiguous or malformed
data instead of guessing intent.

Current limitations:

- localnet uses real Ed25519 user signatures through OpenSSL and real
  BLS12-381 validator signatures through blst;
- localnet key files are deterministic and unencrypted, so they are not
  production-safe key custody;
- P2P, TCP transport, gossip and encrypted peer-channel foundations are present
  for testnet development, but are not production networking yet;
- slashing evidence and validator penalty decisions are implemented as
  auditable, idempotent protocol records; automatic production stake-slashing is
  still out of scope;
- the mempool does not yet implement a full per-account future-nonce queue;
- balance, nonce, minimum fee, chain-bound transaction authorization and
  protocol-domain execution now run through the authoritative state-transition
  engine before protocol-commitment validation can vote on a block;
- coin-lot ownership, double-spend and complete supply audit are still being
  consolidated behind the state-transition validator.

Localnet currently declares an explicit development account allocation in
`GenesisConfig` for the default user key so local block production can validate
balance and nonce. This allocation participates in the deterministic genesis id.
It is not production monetary policy and must be replaced by a reviewed genesis
supply configuration before testnet or mainnet.

The economic preview calculates a deterministic account state root from
canonical account state serialization. The root commits to account addresses,
balances, nonces and the account-state-root format version.

The runtime manifest stores `latestStateRoot`. At genesis it commits to the
initial account allocation and the initial protocol domains. After every
finalized block, it must match the block's `postStateRoot`. Reload rebuilds
state from genesis through finalized blocks with `ProtocolStateTransition`,
advancing account state and protocol domains in the same replay step. Any
manifest, block file or audit result whose root diverges is rejected. The reload
path validates each finalized artifact through domain validators before applying
finalization, then calculates this root through `RuntimeStateVerifier` so loader
and chain audit share the same deterministic full-state check.

`ProtocolInvariantChecker` is the heavy audit boundary after genesis start and
after reload. It checks chain-tip height/hash coherence, deterministic latest
state root calculation, finalized-height bounds, finalized-block linkage,
minimum active validator count, active validator identity binding and penalty
ledger idempotency helpers.

Critical local storage is versioned explicitly. The node data directory must
carry `storage_schema.nodo` for `NODO_NODE_DATA_DIRECTORY` version `1`.
Unknown versions are not loaded and no downgrade or migration is inferred.

`block produce` never creates transactions. Transactions enter the protocol via
`tx submit`, then block production consumes the current mempool contents.
