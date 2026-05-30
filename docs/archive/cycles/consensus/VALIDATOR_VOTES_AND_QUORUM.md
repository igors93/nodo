# Nodo Validator Votes and Quorum Certificate

Status: Cycle 3 Implementation  
Version: NODO-CONSENSUS-VOTE-QC-V1

## Purpose

This phase adds the first consensus voting primitive.

A signed proposal can now receive validator votes, and enough valid votes can
form a `QuorumCertificate`.

## New Components

```text
ValidatorVoteRecord
QuorumCertificate
QuorumCertificateBuilder
QuorumCertificateBuildResult
```

## Vote Security Rules

A vote commits to:

```text
validator address
validator public key
block index
block hash
previous hash
round
decision
reason hash
timestamp
```

The vote is signed over a deterministic versioned payload.

## Quorum Rules

A certificate is built only when:

```text
validator registry is valid
all voters are active registered validators
all votes verify cryptographically
all votes target the same block and round
all votes are APPROVE
no voter appears twice
valid votes reach the configured threshold
```

The default threshold is:

```text
2 / 3 of active validators, rounded up
```

## Why This Matters

This is not full consensus yet, but it creates the central object that future
block finalization should require:

```text
proposed block
        ↓
validator votes
        ↓
quorum certificate
        ↓
finalization candidate
```
