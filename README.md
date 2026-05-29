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
