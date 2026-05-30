# Nodo Validator Registry and Identity Binding

Status: Cycle 1 Foundation  
Version: NODO-VALIDATOR-REGISTRY-V1

## Purpose

This phase adds the first validator identity registry.

In simple terms:

```text
validator public key
        ↓
deterministic Nodo address
        ↓
ValidatorRegistrationRecord
        ↓
ValidatorRegistry
```

## New Components

```text
ValidatorRegistrationRecord
ValidatorRegistryEntry
ValidatorRegistryUpdateResult
ValidatorRegistry
```

## Security Rules

A validator registration is valid only when:

```text
address is a valid Nodo address
public key is valid
address is derived from the same public key
activation epoch is positive
timestamp is positive
metadata hash is safe
```

## Why This Matters

A signed block proposal proves that a key signed something.

But a blockchain also needs to know whether that key belongs to a registered validator.

This registry is the first layer that binds:

```text
validator address ↔ validator public key
```

## What This Does Not Do Yet

This phase does not yet make validator registration mandatory for consensus.

Future phases should connect this registry to:

```text
block proposal validation
validator votes
stake locks
slashing
on-chain validator registration ledger records
```
