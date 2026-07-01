# Nodo Protocol

Nodo Protocol is the deterministic protocol path used by localnet today and by
future testnet and mainnet networks by configuration. It is an experimental
Proof-of-Protection blockchain foundation, not a production mainnet.

The protocol path is:

1. build a candidate block from admitted transactions;
2. validate block structure;
3. validate signatures through the configured provider;
4. validate the state transition without partial mutation;
5. collect validator PREVOTE messages and then PRECOMMIT messages;
6. build a quorum certificate from PRECOMMIT votes only;
7. finalize the certified block through the post-quorum commit pipeline;
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
  auditable, idempotent protocol records; conflicting votes and conflicting
  proposer-signed block proposals become durable, gossipable evidence, and
  finalized evidence now applies deterministic penalties to the validator
  penalty ledger, validator registry and staking registry;
- the mempool exposes only each account's executable nonce frontier to block
  production while future nonces remain queued behind gaps;
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
`tx submit`, then block production consumes the current mempool contents. This
command is a DEVELOPMENT_LOCAL-only helper that may build one local PRECOMMIT QC
for end-to-end persistence testing. Testnet-candidate and production networks
must use the distributed proposer/PREVOTE/PRECOMMIT/QC path and then enter
`RuntimeBlockPipeline::commitCertifiedBlock`.


Validator vote persistence is vote-material based. The node never treats a
boolean `already voted` flag as sufficient recovery proof; the recovery file must
contain the canonical signed PREVOTE/PRECOMMIT record so restart can replay the
same vote without producing a different one.

## Finalized slashing evidence sync

`SLASHING_EVIDENCE` records are part of the finalized block payload. A node that did not receive the original `SLASHING_EVIDENCE_ANNOUNCE` can still verify the evidence during block sync because the canonical state transition replays those records and applies the deterministic penalty. Finalized import and reload audit the resulting `ValidatorPenaltyLedger`, `ValidatorRegistry` and `StakingRegistry` before accepting the synced state root.


## Mandatory P2P admission boundary

The real node networking path does not accept protocol messages directly from transport. `TcpTestnetNodeRuntime` constructs `GossipMesh` with authenticated sessions and eclipse protection enabled. Only `PEER_CHALLENGE` and `PEER_HELLO` are allowed before peer authentication. All other messages must pass encrypted-session authentication, envelope validation, per-peer/per-message-type rate limiting, quarantine checks and eclipse-guarded peer admission before they can reach consensus, sync, mempool or slashing handlers.


## P2P peer exchange

`PEER_EXCHANGE` messages carry canonical, bounded peer candidate lists. They are accepted only after peer authentication and envelope validation. Candidates are persisted separately from authenticated peers, checked with `EclipseGuard`, and retried through `PeerReconnectionPolicy` so peer exchange cannot bypass rate limits, quarantine or handshake validation.
