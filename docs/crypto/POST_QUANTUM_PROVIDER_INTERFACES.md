# Nodo Post-Quantum Provider Interfaces

Status: Implemented Interface Foundation  
Version: NODO-POST-QUANTUM-PROVIDER-INTERFACES-V1

## Purpose

This document describes Nodo's first post-quantum provider interface foundation.

In simple terms:

```text
Nodo does not implement post-quantum cryptography yet.
Nodo now has the doorway where post-quantum providers will plug in later.
```

## Current Components

This phase adds:

```text
PostQuantumSignatureProvider
PostQuantumAlgorithmProfile
PostQuantumMigrationPlan
```

## What This Is

This is an interface and planning phase.

It defines:

- how a future post-quantum signature provider should expose metadata;
- which post-quantum families Nodo currently recognizes;
- how Nodo can describe a future classic + post-quantum migration plan;
- tests proving that the interface can be implemented without touching the blockchain core.

## What This Is Not

This phase does not add real ML-DSA or SLH-DSA signing.

It does not generate real post-quantum keys.

It does not verify real post-quantum signatures.

It does not make Nodo suitable for production use.

## Known Post-Quantum Families

Nodo currently models these candidates:

```text
POST_QUANTUM_ML_DSA
POST_QUANTUM_SLH_DSA
```

Both are marked as:

```text
INTERFACE_ONLY_NO_AUDITED_PROVIDER
```

That is intentional.

## Why This Matters

A blockchain should not hardcode one cryptographic future.

The provider interface prepares Nodo for:

- audited ML-DSA provider integration;
- audited SLH-DSA provider integration;
- hybrid classic + post-quantum signature bundles;
- future migration rules;
- post-quantum readiness without rewriting the core ledger.

## Security Rule

No post-quantum provider should be marked suitable for production use until it is backed by an audited implementation, test vectors, key validation rules, serialization rules, and signature verification tests.
