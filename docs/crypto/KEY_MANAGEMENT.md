# Nodo Key Management Boundary

Status: Implemented Foundation  
Version: NODO-KEY-MANAGEMENT-BOUNDARY-V1

## Purpose

This document describes the first key management boundary in Nodo.

In simple terms:

```text
PrivateKey + PublicKey -> KeyPair -> Address + signatures
```

Before this phase, the project had separate `PublicKey` and `PrivateKey` classes. This phase adds `KeyPair` as the first official place where those two keys are handled together.

## Current Components

This phase adds:

```text
KeyPair
```

The current `KeyPair` can:

- hold a public key and private key together;
- validate that both keys exist;
- validate that both keys use the same algorithm;
- derive a Nodo address from the public key;
- sign messages through a `SignatureProvider`;
- test whether signing and verification works through a provider;
- expose a public identity without leaking private key material.

## Development Key Generation

This phase includes:

```text
KeyPair::createDevelopmentKeyPair(seed)
```

This is deterministic and useful for tests and demos.

It is not secure key generation.

A real wallet must eventually use operating-system randomness, secure storage, encryption, backups, and recovery rules.

## Security Rule

`KeyPair::serializePublic()` and `KeyPair::publicIdentity()` must never include private key material.

Private key material should not appear in logs, README examples, public identities, block data, or network messages.

## Current Limitation

Because the current signature provider is still development-only, `KeyPair` cannot cryptographically prove real private-key ownership yet.

That proof becomes meaningful when Nodo adds a real audited signature provider.

## Next Steps

Future phases should add:

- secure key generation boundary;
- encrypted key storage;
- wallet identity metadata;
- real audited signature provider;
- post-quantum provider interfaces;
- migration from demo labels like `igor` and `ana` to real addresses.
