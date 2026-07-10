# Networks and Data Directory

Nodo networks should differ by configuration and safety policy, not by hidden protocol shortcuts.

## Network profiles

| Network | Purpose | Status |
| --- | --- | --- |
| `localnet` | Local development and testing. | Runnable. |
| `localnet-soak` | Longer-running local testing. | Development profile. |
| `testnet-candidate` | Stricter pre-public-testnet validation. | Runnable. Signing commands require a password-encrypted local key (`TESTNET_SAFE`), created via `nodo keys create --network testnet-candidate`. |
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

## Key custody policy

The same rule applies to every signing command (`node run`, `tx submit`, `governance propose|vote|execute`, `validator exit|unjail`, `stake lock|deposit|top-up|unlock|withdraw`, `keys create`) through `crypto::ProtocolCryptoContext` and `node::ProductionKeySafetyGate`:

- `localnet` / `localnet-soak`: a plaintext local key is accepted.
- `testnet-candidate` / `testnet`: the local key must be password-encrypted to at least `TESTNET_SAFE` (set `NODO_KEY_PASSWORD` or answer the interactive prompt when running `nodo keys create --network testnet-candidate`). There is no external-signer requirement; an encrypted local key is sufficient. See [Key management](../security/key-management.md).
- `mainnet`: unconditionally blocked. No audited key provider (HSM/KMS) exists yet.

## Mainnet rule

Mainnet must reject unsafe development assumptions, including unencrypted local keys, non-canonical storage, incomplete evidence handling, and unreviewed production custody.
