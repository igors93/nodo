# Transactions

All user transactions use the same `TransactionTypePolicyRegistry` support
matrix. Local/network admission and mempool entry use that matrix;
`TransactionExecutionRouter` is the single execution path for block preview,
finalization and replay.

## User transaction types

- `TRANSFER`
- `BURN`
- `STAKE_DEPOSIT`
- `STAKE_TOP_UP`
- `STAKE_UNLOCK`
- `STAKE_WITHDRAW`
- `VALIDATOR_REGISTER`
- `VALIDATOR_EXIT_REQUEST`
- `VALIDATOR_UNJAIL_REQUEST`
- `GOVERNANCE_PROPOSE`
- `GOVERNANCE_VOTE`

Validator registration and governance voting use canonical, length-prefixed
payloads. Governance proposals use the transaction data field; protocol data is
never overloaded into the destination address.

`MINT_REWARD` is represented by authorized reward/mint records, `PENALTY` by
canonical slashing evidence, and validator consensus votes by consensus/P2P
messages. They are not mempool transaction types. The former
`LOCK_SECURITY`/`UNLOCK_SECURITY` transaction labels were removed because stake
positions are the canonical security-lock domain.

## Admission

Admission validates structure, chain binding, sender/key binding, signatures,
minimum fee, nonce conflicts, replacement fee bump, maximum debit and, when a
runtime domain context is available, staking, validator and governance rules.
RPC, CLI and transaction gossip invoke this gate before mutating the mempool.

## Execution and commitments

The router applies account debit/credit and nonce rules once, then dispatches to
the corresponding protocol-domain handler. Domain execution operates on copied
state and commits atomically only after success. Receipts contain the transaction
type, sender, target, amount, fee, nonce transition, touched domains and the
intermediate state commitment.

`STAKE_UNLOCK` starts deterministic unbonding and removes stake from active
validator weight. `STAKE_WITHDRAW` only finalizes a position that has reached
its withdrawable height and credits liquid balance.

The state root commits accounts, supply, burns, staking accounts, staking
positions, pending activation, pending unbonding, lifecycle records, validator
lifecycle and ownership, governance proposals/votes, and slashing state.
Finalization and reload use the same handlers.
