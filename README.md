# Nodo

## Implementation: Validator Proposal Registry and Double-Sign Detection

This phase adds a registry for signed validator block proposals.

New components:

```text
ValidatorProposalRegistryEntry
ValidatorDoubleSignEvidence
ValidatorProposalRegistrationResult
ValidatorProposalRegistry
```

Security behavior:

```text
valid signed proposals are stored
duplicate proposal broadcasts are ignored safely
the same validator signing two block hashes for the same height is detected
invalid signatures are rejected before conflict checks
wrong-chain-tip proposals are rejected
```

New test:

```text
tests/core/ValidatorProposalRegistryTests.cpp
```
