# Transactions and State

Transactions are accepted only if they are valid under the current account and protocol state.

## Transaction admission

Admission should verify:

- transaction shape and canonical serialization;
- signature and public-key material;
- transaction id;
- sender account state;
- nonce;
- minimum fee;
- balance and coin-lot ownership;
- duplicate transaction id;
- duplicate sender/nonce in mempool.

## State transition

The state-transition engine applies transactions and protocol records deterministically. It produces commitments such as:

- account state root;
- protocol-domain roots;
- receipt root;
- post-state root.

Important domains include:

- accounts;
- balances;
- ledger records;
- coin lots;
- staking;
- validators;
- governance;
- treasury;
- rewards;
- penalties;
- slashing evidence.

## Balance origin

Nodo should not treat balance as an unexplained number. Balances must be explainable through origin records:

- genesis allocation;
- mint/emission;
- transfer;
- fee;
- reward;
- burn;
- treasury execution;
- slashing/penalty.

## Receipts

Transaction receipts should provide deterministic evidence of execution result, state effect, fees, and rejection reason when applicable.

## State-root rule

If a block claims a post-state root, replay must reproduce the same root. A mismatch means the block or local storage is invalid.
