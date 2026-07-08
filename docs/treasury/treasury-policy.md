# Treasury Policy

Treasury policy controls when protocol-owned funds may move.

## Policy checks

Treasury execution should verify:

- governance approval exists;
- approval is valid for the requested action;
- proposal has not expired;
- action has not already been executed;
- recipient is valid;
- amount is valid;
- treasury has enough balance;
- epoch/height/timelock rules are satisfied;
- execution is serialized canonically.

## Safety rule

No treasury spend should be possible through a direct shortcut. Execution must pass the same policy and evidence checks regardless of whether it is triggered by CLI, RPC, or internal runtime logic.

## Current status

Treasury policy and execution evidence foundations exist. Real-value treasury use remains blocked until governance, key custody, monitoring, and operator procedures are finalized.
