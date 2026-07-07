# Nodo Audited Signature Provider Boundary

Status: Implemented Integration Boundary  
Version: NODO-AUDITED-SIGNATURE-PROVIDER-BOUNDARY-V1

## Purpose

This document describes Nodo's audited signature provider boundary.

In simple terms:

```text
Nodo now has a gate for real audited signature providers.
```

## Important Honesty Note

This phase does not bundle a real audited Ed25519 or ECDSA library.

That would require choosing a real external implementation, pinning versions, adding official test vectors, reviewing build configuration, and documenting the audit source.

Instead, this phase adds the production gate that a real provider must pass.

## Current Components

This phase adds:

```text
AuditedSignatureProvider
AuditedSignatureProviderProfile
```

## What This Enables

A future provider can now be connected through:

```text
AuditedSignatureProvider
        ↓
providerProfile()
        ↓
production readiness gate
```

The provider profile must include:

- algorithm;
- provider name;
- provider version;
- audit report reference;
- implementation reference;
- production readiness flag.

## Safety Rule

Development signatures must not pass the audited provider gate.

Unverified placeholders must not pass production readiness.

A provider marked `UNVERIFIED` must be rejected for production use.

## Future Work

The next real implementation step is to select and integrate an audited signature backend, such as:

- audited Ed25519 provider;
- audited ECDSA secp256k1 provider;
- OS-backed provider;
- hardware-backed provider.

That integration must include official test vectors and clear dependency documentation.
