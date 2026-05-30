# Validator block proposal signature phase

This phase adds validator signatures to protection block proposals.

New components:

```text
ValidatorBlockProposalSignature
SignedProtectionBlockProposal
ValidatorBlockProposalSigner
```

What this means:

```text
reward block proposals now carry validator identity
the signature commits to the exact block hash
the signature commits to the exact chain tip
signed proposals cannot be replayed on a different proposal
signed proposals cannot be replayed on a different chain tip
development signatures are rejected by future production-like policies
```

Recommended commit:

```bash
git commit -m "Add validator block proposal signature"
```
