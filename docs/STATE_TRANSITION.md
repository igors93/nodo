# State Transition

A block may be voted on only after its transition from the current chain tip is
validated.

Minimum validation rules:

- the current blockchain is valid from genesis;
- the candidate block is structurally valid;
- `previousHash` equals the current chain tip hash;
- block height equals tip height plus one;
- every ledger record is valid;
- duplicate ledger record source ids inside the same block are rejected;
- transaction ledger payloads must decode to the same transaction id as their
  ledger source id;
- transaction nonce zero, invalid amount, negative fee, empty sender/recipient
  and sender-equals-recipient payloads are rejected;
- transaction fee must be at least the configured network minimum;
- when an account-state preview context is provided, sender balance must cover
  the maximum debit registered for the transaction type;
- transaction nonce must equal the sender account nonce plus one;
- duplicate transaction ids inside the same block are rejected;
- selected transactions were already admitted by mempool policy;
- no partial state mutation happens before validation succeeds.

The implementation entry point is `BlockStateTransitionValidator`. In
`BlockValidationMode::ProtocolCommitment` it calls
`StateTransitionEngine::executeBlock`, the consensus-facing authoritative
execution boundary. The engine rejects any context that is not a complete
protocol context: account-state enforcement must be enabled, missing accounts
must be forbidden, chain id and crypto context must be present for chain-bound
signature verification, and the canonical protocol-domain executor must be
available. Legacy state-domain fallback callbacks are not accepted by the engine.

`StateTransitionPreview` remains the non-authoritative preview/simulation
primitive. It delegates every transaction to `TransactionExecutionRouter` and
returns an auditable summary of processed transactions, total fee, touched
accounts and transaction ids without mutating runtime state. Structural tests,
local previews and rebuild helpers may call it directly, but pre-vote,
finalization and protocol-commitment validation must use the engine.

With `StateTransitionPreviewContext`, execution simulates sender debit,
recipient credit, fee collection and sender nonce advancement on a temporary
`AccountStateView`. Successful execution produces a deterministic protocol state
root through `StateRootCalculator`. Accounts, supply, burns, staking,
validators, governance and slashing are committed as canonical domains;
insertion order does not affect the root.

`ProtocolStateTransition` owns canonical replay. It produces a `ProtocolReplayState`
that carries account balances/nonces and all protocol domains — supply, burns,
staking, validators, governance and slashing — after every finalized block.
Reload, manifest verification, artifact import, block production and account-tip
helpers now derive from this same replay state instead of rebuilding accounts and
domains through separate paths. Account-only snapshots may still be used as cache
material, but they are not trusted to prove `latestStateRoot` because they do not
contain the non-account domains included in the protocol commitment.

`BlockStateTransitionValidator` is the single pre-vote protocol gate. Structural
mode uses `StateTransitionPreview` only for non-authoritative checks and never
compares block commitments. Protocol-commitment mode drives
`StateTransitionEngine::executeBlock` and compares the computed `stateRoot` and
`receiptsRoot` against the block header. When coin lot preview is enabled on the
context (`enableCoinLotPreview`), every TRANSFER transaction in the block is also
validated by `CoinLotTransactionValidator::applyTransfer` against a working copy
of the `CoinLotRegistry`. A transfer that would exceed available lots, spend
already-spent lots, or violate ownership rules is rejected before the block
reaches the vote stage. Double-spend within the same block is caught because the
working registry is mutated in order and a lot marked SPENT by an earlier
transaction is unavailable to later ones.

The final state root (`stateRoot`) is computed by
`StateRootCalculator::calculateProtocolStateRoot` over a domain map that, when
coin lot preview is enabled, includes a `coin_lots` domain whose value is a hash
of the serialized registry state after all transactions are applied. This makes
the coin lot registry part of the auditable state commitment: any divergence in
lot ownership, lot amounts, or lot status produces a different state root and
causes `BlockValidationMode::ProtocolCommitment` verification to fail.

`ConsensusEventLoop` enforces an additional guard: a vote is cast only when the
validation context is an authoritative protocol context. That means a valid chain
identifier and crypto context are in place, signatures are actually verified,
account state is enforced, missing-account fallback is disabled and the canonical
protocol-domain executor is available. Proposals that arrive before this context
is initialized are skipped rather than silently accepted.

The correct failure behavior is rejection with a clear reason. A quorum
certificate must never be built for a block that failed this gate.
