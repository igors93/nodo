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
