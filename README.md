# Nodo

---

## Implementation: Validator Block Proposal Signature

The next code step signs protection block proposals with a validator identity.

New components:

```text
ValidatorBlockProposalSignature
SignedProtectionBlockProposal
ValidatorBlockProposalSigner
```

This allows Nodo to verify:

```text
who proposed the reward block
which exact block was signed
which exact chain tip the proposal belongs to
whether the signature matches the proposal payload
```

New test:

```text
tests/core/ValidatorBlockProposalSignatureTests.cpp
```

New documentation:

```text
docs/economics/VALIDATOR_BLOCK_PROPOSAL_SIGNATURE.md
```
