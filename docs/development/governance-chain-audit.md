# Governance lifecycle chain audit

This document describes the governance audit layer wired into `ChainAuditor`.

## Goal

Nodo's core rule is that finalized state must be rebuildable from finalized
history. Governance is part of that rule: proposal decisions, treasury spend
authorization, timelocks, and execution evidence must be independently
verifiable during a fresh chain audit.

## New boundary

`FinalizedGovernanceAudit` is a read-only audit module. It does not decide
proposals and it does not mutate runtime state. It receives:

- the replayed `GovernanceExecutor` from `NodeRuntime`;
- the loaded finalized artifacts;
- the audited tip height from the manifest.

It then verifies:

1. active governance policy snapshots in artifacts match deterministic policy
   construction for the artifact height;
2. active governance action guards match the expected guard set;
3. active governance summaries are structurally valid;
4. the latest artifact's governance summary matches the replayed runtime
   governance state at the audited tip, when that artifact is present;
5. every decided treasury-spend proposal known to the runtime rebuilds into a
   `GovernanceLifecycleRecord` and passes `GovernanceLifecycleVerifier`;
6. every treasury execution evidence embedded in finalized artifacts carries a
   governance lifecycle context and that lifecycle verifies independently;
7. embedded treasury lifecycle evidence must match the lifecycle rebuilt from
   runtime state when the runtime still has that proposal available.

## Why this matters

A treasury spend record by itself is not authorization. It only says funds moved.
For a blockchain audit, the node must also prove why they were allowed to move:
which proposal approved the spend, which votes backed the decision, whether the
tally was valid, whether the timelock was respected, and whether the final
execution evidence is canonical.

By integrating this into `ChainAuditor`, a fresh reload can reject histories that
carry malformed governance summaries or treasury execution evidence with missing
or mismatched governance context.

## Pruned nodes

The audit is compatible with pruning. If an old execution artifact has been
pruned, the auditor verifies the runtime-rebuilt lifecycle for the proposal but
cannot require the old embedded evidence to be present. If the execution height's
artifact is loaded, the embedded evidence is required.
