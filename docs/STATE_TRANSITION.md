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

The implementation entry point is `BlockStateTransitionValidator`. It calls
`StateTransitionPreview`, which delegates every transaction to
`TransactionExecutionRouter`, before accepting a candidate block. The preview returns
an auditable summary of processed transactions, total fee, touched accounts and
transaction ids without mutating runtime state. With `StateTransitionPreviewContext`
it also simulates sender debit, recipient credit, fee collection and sender
nonce advancement on a temporary `AccountStateView`. Successful previews produce
a deterministic protocol state root through `StateRootCalculator`. Accounts,
supply, burns, staking, validators, governance and slashing are committed as
canonical domains; insertion order does not affect the root.

`BlockStateTransitionValidator` is the single pre-vote protocol gate. It drives
`StateTransitionEngine::executeBlock`, which runs `StateTransitionPreview` under
the provided `StateTransitionPreviewContext`. When coin lot preview is enabled on
the context (`enableCoinLotPreview`), every TRANSFER transaction in the block is
also validated by `CoinLotTransactionValidator::applyTransfer` against a working
copy of the `CoinLotRegistry`. A transfer that would exceed available lots, spend
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
validation context has `protocolAuthorizationEnabled()` — meaning a valid chain
identifier and crypto context are in place and signatures were actually verified.
Proposals that arrive before the context is initialized are skipped rather than
silently accepted.

The correct failure behavior is rejection with a clear reason. A quorum
certificate must never be built for a block that failed this gate.
