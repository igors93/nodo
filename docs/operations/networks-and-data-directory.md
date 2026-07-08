# Networks and Data Directory

Nodo networks should differ by configuration and safety policy, not by hidden protocol shortcuts.

## Network profiles

| Network | Purpose | Status |
| --- | --- | --- |
| `localnet` | Local development and testing. | Runnable. |
| `localnet-soak` | Longer-running local testing. | Development profile. |
| `testnet-candidate` | Stricter pre-public-testnet validation. | Foundation exists. |
| `mainnet` | Future production network. | Blocked. |

## Network parameters

A network profile should define:

- chain id;
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

## Data directory

The default local data directory is `.nodo`. See [Storage and reload](../architecture/storage-and-reload.md) for the safety model.

## Mainnet rule

Mainnet must reject unsafe development assumptions, including unencrypted local keys, non-canonical storage, incomplete evidence handling, and unreviewed production custody.
