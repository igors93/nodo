# Epoch reward block proposal phase

This phase connects epoch reward distribution to deterministic block proposal.

New components:

```text
EpochRewardLedgerBuildResult
EpochRewardLedgerBuilder
ProtectionBlockProposal
ProtectionBlockBuilder
```

What this means:

```text
reward distributions can now become official LedgerRecords
reward LedgerRecords are ordered canonically
a protection reward block can be proposed for the next chain index
the proposal is bound to the current chain tip
no-reward epochs still produce an auditable ProtectionEpoch block
```

Recommended commit:

```bash
git commit -m "Connect epoch rewards to block proposal"
```
