# Governance Vote Evidence

A governance decision should not be accepted unless it can be rebuilt from vote evidence.

## Required vote context

A vote record should bind:

- proposal id;
- voter identity;
- validator or voting weight context;
- vote choice;
- signature/public key material;
- height or epoch context;
- fee/accounting effect when applicable.

## Verification

Vote verification should check:

- valid proposal id;
- valid voter identity;
- valid signature;
- no duplicate vote unless replacement rules allow it;
- valid voting period;
- valid weight context;
- canonical serialization.

## Tally

A tally should be deterministic. Given the same proposal and same canonical vote set, all nodes must compute the same result.

## Audit rule

Governance state should be rejected if the recorded decision cannot be reproduced from the proposal, vote evidence, tally rules, and execution record.
