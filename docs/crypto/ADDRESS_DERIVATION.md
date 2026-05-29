# Nodo Address Derivation

Status: Implemented Foundation  
Version: NODO-ADDRESS-DERIVATION-V1

## Purpose

This document describes the first deterministic address derivation foundation in Nodo.

In simple terms:

```text
PublicKey -> deterministic address
```

Before this phase, demo accounts still used simple names such as `igor` and `ana`.
This phase does not replace the account system yet. It creates the cryptographic address foundation that future wallet and transaction phases can use.

## Current Format

Current development address shape:

```text
nodo1 + 40 lowercase hexadecimal payload characters + 8 checksum characters
```

Example shape:

```text
nodo1aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
```

This is not the final production wallet address format. It is a deterministic development foundation.

## Derivation Rule

The address payload is derived from:

```text
NODO_ADDRESS_DERIVATION_V1|PublicKey{...}
```

using Nodo's SHA-256 hash boundary.

The checksum is derived from:

```text
NODO_ADDRESS_CHECKSUM_V1|<address body>
```

## Validation

An address is valid only if:

- it has the `nodo1` prefix;
- it has the expected length;
- it uses lowercase hexadecimal characters after the prefix;
- its checksum matches the body.

## Why This Matters

This prepares Nodo for:

- wallet addresses;
- replacing plain account labels such as `igor` and `ana`;
- verifying whether an address belongs to a public key;
- future transaction payloads that use addresses instead of names;
- future key management.

## Current Security Note

This is a foundation, not a final production format.

Future work may migrate to an audited address encoding format such as Bech32/Base32 and add network identifiers, version bytes, key type markers, and stronger checksum rules.
