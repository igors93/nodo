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
development signature under production-like policy
```

## Development Mode

The current test path uses `DevelopmentSignatureProvider`.

That is not production cryptography.

It is used only to keep the signing architecture testable while real audited providers are still evolving.

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
development signatures are rejected by future production-like policy
invalid signature inputs are rejected
```
