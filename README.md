# Nodo

---

## Implementation: Explicit Transaction CoinLot Inputs

The next code step makes transaction input intent explicit.

New behavior:

```text
TRANSFER transactions can declare exactly which CoinLots they want to spend
transaction ids commit to the declared input lots
validators must not silently replace declared inputs with other available lots
legacy automatic-input transactions remain supported for compatibility
```

New test:

```text
tests/core/TransactionExplicitInputTests.cpp
```

New documentation:

```text
docs/economics/EXPLICIT_TRANSACTION_COIN_LOT_INPUTS.md
```

---

## Implementation: GenesisReward Main State Flow

The next code step connects `GenesisRewardRecord` to the main public `State`.

New behavior:

```text
State can apply GenesisRewardRecord directly
GENESIS_REWARD ledger records can rebuild public State
GenesisReward creates deterministic reward CoinLots
legacy MintRecord remains for compatibility only
```

New test:

```text
tests/core/GenesisRewardStateFlowTests.cpp
```

New documentation:

```text
docs/economics/GENESIS_REWARD_MAIN_STATE_FLOW.md
```

---

## Implementation: Epoch Reward Distributor

The next code step adds the first reward distributor for protection epochs.

New components:

```text
ValidatorRewardShare
EpochRewardDistribution
EpochRewardDistributor
```

This allows Nodo to calculate:

```text
accepted validator work
validator score
work demand
emission cap
security emission
GenesisReward records
```

Important monetary safety rule:

```text
GenesisReward mints only securityEmission.
feesCollected are existing coins and must not be minted again.
```

New test:

```text
tests/economics/EpochRewardDistributorTests.cpp
```

New documentation:

```text
docs/economics/EPOCH_REWARD_DISTRIBUTOR.md
```

---

## Implementation: Epoch Reward Block Proposal

The next code step connects reward distribution to block proposal.

New components:

```text
EpochRewardLedgerBuildResult
EpochRewardLedgerBuilder
ProtectionBlockProposal
ProtectionBlockBuilder
```

This allows Nodo to create a deterministic block from:

```text
ProtectionEpoch
GenesisReward records
current blockchain tip
```

New test:

```text
tests/economics/EpochRewardBlockProposalTests.cpp
```

New documentation:

```text
docs/economics/EPOCH_REWARD_BLOCK_PROPOSAL.md
```
