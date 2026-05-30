# Nodo Validator Proposal Admission

Status: Cycle 2 Implementation  
Version: NODO-VALIDATOR-PROPOSAL-ADMISSION-V1

## Purpose

This phase connects the validator identity registry to signed block proposal validation.

Before this phase, a signed proposal proved:

```text
this key signed this proposal
```

After this phase, admission also checks:

```text
this key belongs to a registered active validator
```

## New Components

```text
ValidatorProposalAdmissionStatus
ValidatorProposalAdmissionResult
ValidatorProposalAdmissionPolicy
```

## Admission Flow

```text
SignedProtectionBlockProposal
        ↓
blockchain tip validation
        ↓
signature verification
        ↓
ValidatorRegistry identity check
        ↓
ValidatorProposalRegistry registration
        ↓
double-sign detection
```

## Security Rules

A signed proposal is admitted only when:

```text
blockchain is valid
validator registry is valid
proposal is valid for the current chain tip
signature verifies under the active crypto policy
validator address is registered
validator is ACTIVE
proposal public key matches the registry public key
proposal registry is valid
proposal does not trigger unsafe registry mutation
```

## Rejection Cases

```text
INVALID_BLOCKCHAIN
INVALID_VALIDATOR_REGISTRY
INVALID_SIGNED_PROPOSAL
UNREGISTERED_VALIDATOR
INACTIVE_VALIDATOR
VALIDATOR_PUBLIC_KEY_MISMATCH
INVALID_PROPOSAL_REGISTRY
REGISTRATION_FAILED
DOUBLE_SIGN_CONFLICT
```

## Why This Matters

A blockchain cannot accept validator actions just because a signature exists.

It must also prove that the signer is an allowed validator identity.

This phase prepares Nodo for:

```text
validator vote admission
consensus round validation
stake-weighted proposer selection
slashing connected to registered validator identity
```
