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

- localnet uses `KeyStore`, `Signer` and `LocalSignatureProvider`, but the
  provider is still a temporary deterministic local provider;
- localnet key files are unencrypted and are not production-safe;
- P2P networking is intentionally out of scope for this foundation step;
- no slashing is implemented yet; invalid quorum/finalization evidence is
  rejected and reported only;
- the mempool does not yet implement a full per-account future-nonce queue;
- balance, nonce and minimum fee checks now run inside the state-transition
  preview before votes;
- coin-lot ownership, double-spend and complete supply audit are still being
  consolidated behind the state-transition validator.

Localnet currently declares an explicit development account allocation in
`GenesisConfig` for bootstrap validators so local block production can validate
balance and nonce. This allocation participates in the deterministic genesis id.
It is not production monetary policy and must be replaced by a reviewed genesis
supply configuration before testnet or mainnet.

The economic preview calculates a deterministic account state root from
canonical account state serialization. The root commits to account addresses,
balances, nonces and the account-state-root format version.

The runtime manifest stores `latestStateRoot`. At genesis it commits to the
initial account allocation. After every finalized block, it must match the
block's `postStateRoot`. Reload rebuilds state from genesis through finalized
blocks and rejects any manifest, block file or audit result whose root diverges.

`block produce` never creates transactions. Transactions enter the protocol via
`tx submit`, then block production consumes the current mempool contents.
