# Transactions

Transactions enter Nodo through `tx submit` and are validated before persistence.

## Current Localnet Checks

The localnet transaction path checks:

- sender and recipient fields;
- signature validity;
- duplicate transactions;
- duplicate sender nonce;
- old nonces;
- unsupported future nonces;
- minimum fee;
- account balance;
- state-transition preview before block finality.

## Mempool

The persistent mempool stores admitted transactions. Block production consumes mempool contents; it does not create transactions automatically.

## Current Limitations

- A full per-account future-nonce queue is not complete.
- Wallet and production custody workflows are not ready.
- Mainnet transaction operation remains blocked with the rest of mainnet.
