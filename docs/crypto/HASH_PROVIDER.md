# Nodo Hash Provider

Status: Implemented Foundation  
Version: NODO-HASH-PROVIDER-V1

## Purpose

This document describes Nodo's current hash boundary.

In simple terms:

```text
old hash = development placeholder
new hash = SHA-256 provider boundary
```

## Current Decision

Nodo now uses SHA-256 through the C hash boundary:

```text
include/crypto/hash.h
src/crypto/hash.c
```

The public API returns 64 lowercase hexadecimal characters.

## Why This Matters

Hashes are used across Nodo for:

- block hashes;
- ledger record payload hashes;
- transaction identifiers;
- mint record identifiers;
- storage manifest hashes;
- block index hashes;
- privacy development commitments and nullifiers;
- development signatures.

Replacing the old placeholder hash improves the foundation before moving deeper into real signatures, addresses, and key management.

## Important Security Note

This is a real SHA-256 implementation with known-vector tests, but Nodo is still not production-ready.

Before real funds, the hash boundary should eventually be reviewed, fuzz-tested, and possibly backed by a widely audited cryptographic library/provider.

## Test Coverage

The crypto test checks known SHA-256 vectors:

- empty string;
- `abc`;
- `hello`;
- `The quick brown fox jumps over the lazy dog`.

It also verifies:

- deterministic output;
- 64-character lowercase hex output;
- byte API matches string API.
