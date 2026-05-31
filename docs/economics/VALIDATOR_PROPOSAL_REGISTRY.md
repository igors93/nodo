# Nodo Validator Proposal Registry and Double-Sign Detection

Status: Implementation Guide  
Version: NODO-VALIDATOR-PROPOSAL-REGISTRY-V1

## Purpose

This phase adds a local registry for signed validator block proposals and detects double-signing.

In simple terms:

```text
if the same validator signs two different block proposals for the same height,
Nodo records that as double-sign evidence
```

## New Components

```text
ValidatorProposalRegistryEntry
ValidatorDoubleSignEvidence
ValidatorProposalRegistrationResult
ValidatorProposalRegistry
```

## What Is Stored

The registry stores compact proposal evidence:

```text
validator address
validator public key fingerprint
block index
block hash
previous hash
chain size before proposal
expected previous hash
proposal timestamp
signature digest
```

## Double-Sign Rule

A conflict is detected when:

```text
same validator
same public key fingerprint
same block index
different block hash
```

The conflicting proposal is not added as a normal accepted entry. Instead, the registry records `ValidatorDoubleSignEvidence`.

## Security Rules

The registry rejects:

```text
proposal invalid for the current chain tip
signature that does not verify
proposal signed for another chain tip
single non-hybrid validator signature under future hybrid policy
unsafe validator address text
invalid registry entries
```

## Why This Matters

This is the first protection against a validator trying to support two competing histories at the same height.

Future slashing logic can use `ValidatorDoubleSignEvidence` to reduce score, slash locked CoinLots or ban a validator.

## What This Does Not Do Yet

This phase does not yet slash the validator automatically.

The next natural phases are:

```text
turn double-sign evidence into penalty records
connect penalty records to validator score
connect severe conflicts to CoinLot slashing
```

## New Test

```text
tests/core/ValidatorProposalRegistryTests.cpp
```
