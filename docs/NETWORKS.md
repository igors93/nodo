# Networks

Nodo networks differ by configuration, not by protocol code path.

Required network parameters:

- `chainId`;
- network name;
- protocol version;
- genesis config id;
- minimum validator count;
- quorum threshold;
- maximum transactions per block;
- maximum mempool transactions;
- minimum fee;
- target block time;
- finality depth;
- signature algorithm policy;
- storage format version.

`localnet` is the only runnable network profile in this repository today. It
uses the same runtime, key-store, signer, mempool, block production, validation,
consensus, finalization, persistence and reload path intended for future
networks.

`testnet` should use production-shaped keys, stricter validator onboarding and
networked validator votes.

`mainnet` must refuse unencrypted local keys, non-canonical storage and
incomplete security evidence handling.
