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

Keys are still derived deterministically from `genesisConfigId#keyType#keyId` (`crypto::KeyStore::createLocalKey`), not from real randomness, on every network profile including `testnet-candidate`. This is a known limitation distinct from encryption-at-rest and is not solved by the `TESTNET_SAFE` bar above.

`crypto::OutofProcessSigner` (an out-of-process/external-signer primitive) exists in the codebase but is not wired into any command; it is not part of the enforced policy.
