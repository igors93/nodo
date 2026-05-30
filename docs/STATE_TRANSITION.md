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
- selected transactions were already admitted by mempool policy;
- no partial state mutation happens before validation succeeds.

The implementation entry point is `BlockStateTransitionValidator`. It is the
single pre-vote protocol gate and should grow to include signature, nonce,
balance, coin-lot, fee and supply checks as those state models become canonical.

The correct failure behavior is rejection with a clear reason. A quorum
certificate must never be built for a block that failed this gate.
