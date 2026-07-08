# Governance Overview

Nodo governance exists to make protocol decisions auditable.

## Governance goals

- proposals are recorded canonically;
- votes are signed and attributable;
- tally rules are deterministic;
- decisions can be rebuilt from evidence;
- execution is linked to an approved decision;
- treasury execution requires governance evidence when applicable.

## Proposal types

Current governance foundations support the following proposal categories:

- parameter change;
- treasury spend;
- text proposal.

## Lifecycle

```text
proposal
   ↓
vote evidence
   ↓
vote-set audit
   ↓
tally
   ↓
decision
   ↓
execution
   ↓
lifecycle audit
```

## Public governance status

The implementation contains foundations for proposal, voting, decision, execution, and audit. Public governance is not final until quorum, timelocks, emergency powers, cancellation, veto, and operator procedures are fully specified.
