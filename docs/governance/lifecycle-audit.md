# Governance Lifecycle Audit

A governance lifecycle record stores the proposal, policy, votes, tally, decision, and lifecycle metadata needed to verify an official decision.

## Lifecycle Contents

- lifecycle id;
- proposal envelope;
- governance policy;
- voting policy;
- vote evidence list;
- tally report;
- decision record;
- created block;
- finalized block.

## Verification Flow

```text
validate lifecycle structure
-> audit vote evidence set
-> rebuild tally from canonical votes
-> compare rebuilt tally with stored tally
-> rebuild expected decision
-> compare rebuilt decision with stored decision
-> accept or reject
```

## Treasury Approval Bridge

`GovernanceApprovalBridge::produceTreasuryApprovalFromVerifiedLifecycle` is the production path. It verifies the lifecycle, requires an approved decision, checks the governance review period, and produces a deterministic treasury approval.

Structural decision approval helpers are test-only and must not be used by production validators.
