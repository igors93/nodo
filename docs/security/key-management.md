# Key Management

Key management is one of the main blockers for production readiness.

## Current local keys

Nodo supports local development keys and encrypted key-file foundations. These are useful for localnet and testnet-candidate development, but they are not enough for production custody.

## Key types

Common key roles include:

- user/account key;
- validator signing key;
- peer identity key;
- governance signing key;
- treasury execution key or signer boundary.

## Safety requirements

Production-ready operation requires:

- encrypted key storage by default;
- no unsafe default local keys on production networks;
- external signer/HSM boundary;
- key rotation;
- revocation or validator exit workflow;
- backup and recovery policy;
- audit logs for signer use;
- separation between validator, peer, governance, and treasury authority.

## Operator rule

Do not use local development keys for any production-like network with real value.

## Enforced policy today

The CLI enforces one unified rule across every signing command, via `crypto::ProtocolCryptoContext` (crypto provider/policy validity) and `node::ProductionKeySafetyGate` + `crypto::KeyEncryptionPolicy` (key custody):

| Network | Key requirement |
| --- | --- |
| `localnet` / `localnet-soak` | Plaintext local key (`KeyEncryptionLevel::PLAINTEXT`) is accepted. |
| `testnet-candidate` / `testnet` | Local key must be password-encrypted to at least `TESTNET_SAFE`. Run `nodo keys create --network testnet-candidate` and supply a password (via the `NODO_KEY_PASSWORD` environment variable or the interactive prompt, minimum 8 characters). No external signer is required — an encrypted local key is sufficient. |
| `mainnet` | Unconditionally blocked. No audited HSM/KMS provider exists yet; this is enforced independently at the network-profile, crypto-context, and key-safety layers. |

On official networks (`testnet-candidate`, `testnet`), `nodo keys create` generates keys from the operating system CSPRNG (`crypto::KeyStore::createRandomKey`). On `localnet` and `localnet-soak`, keys are still derived deterministically from `genesisConfigId#keyType#keyId` or the localnet seeds (`crypto::KeyStore::createLocalKey`), so development flows stay reproducible; those keys are derivable from public data and must never hold value.

`testnet-candidate` has no built-in genesis, so no bootstrap validator key is derivable from the source code. Operators create their keys before the genesis exists and share only public keys; see [the genesis ceremony](../operations/networks-and-data-directory.md#testnet-candidate-genesis-ceremony).

`crypto::OutofProcessSigner` (an out-of-process/external-signer primitive) exists in the codebase but is not wired into any command; it is not part of the enforced policy.
