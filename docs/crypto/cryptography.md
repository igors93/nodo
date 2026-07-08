# Cryptography

Nodo uses cryptographic providers through explicit boundaries so algorithms and policies can evolve without hidden protocol shortcuts.

## Current foundations

The project includes foundations for:

- address derivation;
- Ed25519 user/peer signatures through OpenSSL;
- BLS12-381 validator signatures through BLST;
- signature-provider interfaces;
- audited signature-provider boundary;
- hash-provider boundary;
- crypto policy and suite identifiers;
- post-quantum provider interface placeholders.

## Address derivation

Addresses should be derived deterministically from public key material and network/policy context. Address validation should reject malformed, unknown, or unsafe formats.

## Signature policy

Every signed protocol object should bind the correct context. A signature should not be valid across unrelated domains if replay would be unsafe.

## Post-quantum boundary

Post-quantum interfaces are architectural boundaries, not a claim that post-quantum production signatures are active. Any future post-quantum provider must be specified, audited, and compatibility-tested before activation.

## Production warning

Cryptographic primitives and key custody must be externally reviewed before mainnet activation.
