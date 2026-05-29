# Nodo Signature Provider Boundary

Status: Implemented Foundation  
Version: NODO-SIGNATURE-PROVIDER-BOUNDARY-V1

## Purpose

This document describes the first official signature provider boundary in Nodo.

In simple terms:

```text
Before:
blockchain code created development signatures directly

Now:
blockchain code asks a provider to sign and verify
```

## Current Components

This phase adds:

```text
SignatureProvider
DevelopmentSignatureProvider
SignatureVerificationResult
```

The current provider is still development-only. It is not real blockchain-grade cryptography.

## Why This Matters

Real signatures should not be hardcoded into the blockchain core.

The blockchain should ask:

```text
provider.sign(message, publicKey, privateKey, timestamp)
provider.verify(message, signature)
```

This prepares Nodo for future providers such as:

- Ed25519;
- ECDSA secp256k1;
- ML-DSA;
- SLH-DSA;
- hybrid classic + post-quantum signature bundles.

## Development Provider Warning

`DevelopmentSignatureProvider` is deterministic and useful for tests, but it does not prove private-key ownership.

It exists only to make the architecture real before adding audited cryptographic providers.

## New SignatureBundle Behavior

`SignatureBundle` now supports:

```text
createSignature(...)
createDevelopmentSignature(...)
verifyForPolicy(...)
```

This means a bundle can now be checked against:

- the message;
- the network crypto policy;
- the security context;
- the provider verification result.

## Security Direction

This phase does not make Nodo production-ready.

It creates the doorway where real signature providers will later be connected.
