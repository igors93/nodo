# Treasury Execution Evidence

Treasury execution evidence proves that a treasury movement was authorized and applied correctly.

## Evidence should include

- proposal id;
- governance decision id;
- execution id;
- recipient;
- amount;
- treasury balance before/after;
- height/epoch context;
- policy checks passed;
- signer/executor context;
- ledger record;
- state commitment effect.

## Governance requirement

Treasury execution must be linked to governance lifecycle evidence unless the protocol explicitly defines a narrow emergency path. Any emergency path must be separately documented, bounded, and auditable.

## Audit rule

A finalized block containing treasury execution should be rejected if the treasury effect cannot be reproduced from policy, governance evidence, ledger records, and state transition.
