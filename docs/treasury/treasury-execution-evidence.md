# Treasury Execution Evidence

Treasury execution evidence binds a spend record to the context that authorized it.

## Evidence Includes

- evidence id;
- treasury proposal;
- treasury approval;
- treasury policy;
- treasury account state before spend;
- current block height;
- epoch spend so far;
- computed spend record;
- governance approval context.

## Governance Requirement

Production treasury evidence must carry a governance lifecycle. The validator rebuilds the lifecycle approval and compares it against the stored treasury approval.

This prevents a direct or manually invented `TreasuryApproval` from being accepted as production evidence.

## Finalized Treasury Audit

Finalized treasury sections reject non-empty spend-only sections. Non-empty treasury activity must be evidence-backed so reload and chain audit can reproduce the authorization path.
