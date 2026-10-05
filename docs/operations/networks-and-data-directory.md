# Networks and Data Directory

Nodo networks should differ by configuration and safety policy, not by hidden protocol shortcuts.

## Network profiles

| Network | Purpose | Status |
| --- | --- | --- |
| `localnet` | Local development and testing. | Runnable. |
| `localnet-soak` | Longer-running local testing. | Development profile. |
| `testnet-candidate` | Stricter pre-public-testnet validation. | Runnable from an operator genesis document (no built-in genesis). Signing commands require a password-encrypted local key (`TESTNET_SAFE`), created from the OS CSPRNG via `nodo keys create --network testnet-candidate`. |
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

## testnet-candidate genesis ceremony

`testnet-candidate` has no genesis in the binary. A genesis built from seeds in the source code would let anyone recompute every bootstrap validator's private key, so the genesis is an operator document assembled from public keys that were generated outside the code:

1. Each bootstrap validator operator creates random keys in their own data directory, before any genesis exists:

   ```bash
   export NODO_KEY_PASSWORD='...'   # or answer the prompt
   nodo keys create --network testnet-candidate --data-dir ./node --type validator --key-id local-validator
   nodo keys create --network testnet-candidate --data-dir ./node --type user --key-id validator-owner
   ```

   Each command prints a `Public key` and an `Address`. The operator shares only the validator public key and the owner address.

2. A coordinator builds the genesis document (at least 4 validators; the file is never overwritten):

   ```bash
   nodo genesis create --network testnet-candidate --output genesis.nodo \
     --genesis-validator <BLS_PUBLIC_KEY_HEX>:<OWNER_ADDRESS>   # once per validator
     --genesis-account <ADDRESS>:<BALANCE_RAW>                  # optional, repeatable
   ```

3. Every operator checks the printed genesis id, then initializes from the same file:

   ```bash
   nodo genesis inspect --genesis-file genesis.nodo
   nodo init --network testnet-candidate --data-dir ./node --genesis-file genesis.nodo
   ```

`init` copies the document to `{dataDir}/genesis.nodo` and records its genesis id in the manifest. Later commands read that copy; editing it changes the genesis id and the node refuses to start.

The document carries only the genesis timestamp, memo, validator public keys with owners, and funded accounts. Quorum, fees, and limits always come from the `testnet-candidate` profile in code, and the document's chain id and protocol version must match it.

## Data directory

The default local data directory is `.nodo`. See [Storage and reload](../architecture/storage-and-reload.md) for the safety model.

## Key custody policy

The same rule applies to every signing command (`node run`, `tx submit`, `governance propose|vote|execute`, `validator exit|unjail`, `stake lock|deposit|top-up|unlock|withdraw`, `keys create`) through `crypto::ProtocolCryptoContext` and `node::ProductionKeySafetyGate`:

- `localnet` / `localnet-soak`: a plaintext local key is accepted.
- `testnet-candidate` / `testnet`: the local key must be password-encrypted to at least `TESTNET_SAFE` (set `NODO_KEY_PASSWORD` or answer the interactive prompt when running `nodo keys create --network testnet-candidate`). There is no external-signer requirement; an encrypted local key is sufficient. See [Key management](../security/key-management.md).
- `mainnet`: unconditionally blocked. No audited key provider (HSM/KMS) exists yet.

## Mainnet rule

Mainnet must reject unsafe development assumptions, including unencrypted local keys, non-canonical storage, incomplete evidence handling, and unreviewed production custody.
