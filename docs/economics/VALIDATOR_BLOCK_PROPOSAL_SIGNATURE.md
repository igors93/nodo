# Nodo Validator Block Proposal Signature

Status: Implementation Guide  
Version: NODO-VALIDATOR-BLOCK-PROPOSAL-SIGNATURE-V1

## Purpose

This document describes the phase where a protection block proposal becomes signed by a validator.

In simple terms:

```text
a reward block is no longer just created by code
it now has a validator identity and a signature attached to it
```

## New Components

This phase adds:

```text
ValidatorBlockProposalSignature
SignedProtectionBlockProposal
ValidatorBlockProposalSigner
```

## What Gets Signed

The validator signs a deterministic payload containing:

```text
payload version
validator address
validator public key
validator public key fingerprint
block index
block hash
previous hash
chain size before proposal
expected previous hash
ledger build result
proposal timestamp
```

## Why This Matters

A proposal signature must be tied to:

```text
the validator
the exact block
the exact chain tip
the exact reward ledger records
```

That prevents replaying a valid signature on another block or another fork.

## Security Rules

This phase rejects:

```text
empty validator address
unsafe validator address delimiters
invalid public key
empty signature bundle
zero timestamp
signature that does not match the proposal
proposal signed for another chain tip
single non-hybrid validator signature under future hybrid policy
```

## Localnet Mode

The current localnet path signs validator block proposals with BLS12-381
through blst and domain `NODO_VALIDATOR_BLOCK_PROPOSAL_V1`.

## Production Direction

Future production mode should require:

```text
real audited signature provider
validator registry lookup
public key bound to validator address
hybrid classic + post-quantum signature policy
proposal voting and quorum
slashing for conflicting signed proposals
```

## New Test

```text
tests/core/ValidatorBlockProposalSignatureTests.cpp
```

The test checks:

```text
signed proposal verifies and appends
signature cannot be reused on another proposal
signature cannot be reused on another chain tip
payload commits to validator identity and block hash
single BLS signatures are rejected by future hybrid policy
invalid signature inputs are rejected
```
