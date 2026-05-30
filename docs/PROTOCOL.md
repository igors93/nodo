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

- localnet still uses a development signature provider;
- key-store commands are declared but not implemented;
- P2P networking is intentionally out of scope for this foundation step;
- balance, nonce and coin-lot transition checks are still being consolidated
  behind the state-transition validator.
