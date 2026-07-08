# Governance Lifecycle Audit

Governance lifecycle audit verifies that each proposal moved through valid states.

## Lifecycle contents

A complete lifecycle should contain:

- proposal record;
- vote records;
- vote-set audit result;
- tally record;
- decision record;
- execution record, if executed;
- rejection or expiration record, if not executed.

## Verification flow

```text
proposal exists
   ↓
votes are valid
   ↓
tally is reproducible
   ↓
decision matches tally
   ↓
execution matches decision and policy
```

## Treasury bridge

Treasury execution should be accepted only if the governance lifecycle proves that the spend was approved and not expired, cancelled, or already executed.

## Open work

Public governance still needs final rules for:

- quorum;
- approval thresholds;
- timelocks;
- emergency execution;
- veto or cancellation;
- parameter-change safety limits;
- validator eligibility for voting.
