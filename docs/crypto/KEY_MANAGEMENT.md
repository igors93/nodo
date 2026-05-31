# Nodo Key Management Boundary

Status: Implemented  
Version: NODO-KEY-MANAGEMENT-BOUNDARY-V2

`KeyPair` binds a public key and private key, derives the Nodo address from the
public key, and signs only through a `SignatureProvider`.

Current key generation helpers:

- `KeyPair::createEd25519KeyPair()`
- `KeyPair::createDeterministicEd25519KeyPair(seed)`
- `KeyPair::createBls12381KeyPair()`
- `KeyPair::createDeterministicBls12381KeyPair(seed)`

`KeyStore` writes localnet key files with explicit metadata:

- `keyType=USER` for Ed25519 transaction keys;
- `keyType=VALIDATOR` for BLS12-381 validator keys;
- `suite=NODO_CRYPTO_SUITE_V1`;
- provider id for OpenSSL Ed25519 or blst BLS12-381.

Localnet deterministic keys are useful for repeatable tests and demos, but the
files are unencrypted and are not production key custody.
