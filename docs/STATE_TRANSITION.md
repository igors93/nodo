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
  `amount + fee`;
- transaction nonce must equal the sender account nonce plus one;
- duplicate transaction ids inside the same block are rejected;
- selected transactions were already admitted by mempool policy;
- no partial state mutation happens before validation succeeds.

The implementation entry point is `BlockStateTransitionValidator`. It calls
`StateTransitionPreview` before accepting a candidate block. The preview returns
an auditable summary of processed transactions, total fee, touched accounts and
transaction ids without mutating runtime state. With `StateTransitionPreviewContext`
it also simulates sender debit, recipient credit, fee collection and sender
nonce advancement on a temporary `AccountStateView`.

`BlockStateTransitionValidator` is the single pre-vote protocol gate and should
grow to include signature, coin-lot ownership, double-spend and complete supply
checks as those state models become canonical. The current preview records total
fees, touched accounts and resulting account states as preparation for full
supply audit. Coin lot preview and full supply reconciliation are explicit next
steps.

The correct failure behavior is rejection with a clear reason. A quorum
certificate must never be built for a block that failed this gate.
