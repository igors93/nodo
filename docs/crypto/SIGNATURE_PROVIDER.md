# Nodo Signature Provider Boundary

Status: Implemented  
Version: NODO-SIGNATURE-PROVIDER-BOUNDARY-V2

## Purpose

Blockchain code signs and verifies through `SignatureProvider`; it does not
implement algorithm internals inline.

## Current Providers

- `Ed25519SignatureProvider`: OpenSSL Ed25519 for user transaction signatures.
- `Bls12381SignatureProvider`: blst BLS12-381 for validator votes, quorum
  certificates and block proposals.

Every signature carries:

- crypto suite: `NODO_CRYPTO_SUITE_V1`;
- signing domain, such as `NODO_TX_V1` or `NODO_VALIDATOR_VOTE_V1`;
- algorithm;
- public key fingerprint;
- signature bytes as hex.

## Policy

`CryptoPolicy` accepts:

- Ed25519 only for `USER_TRANSACTION`;
- BLS12-381 only for validator, mint and treasury operations.

The legacy `DEVELOPMENT_FAKE_SIGNATURE` enum is retained only so old or
malformed data can be rejected explicitly. No fake signature provider is
compiled into the normal protocol path.
