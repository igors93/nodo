# GenesisReward main State flow phase

This phase starts replacing demo MintRecord as the main supply-creation path.

New behavior:

```text
State can apply GenesisRewardRecord directly
ChainStateRebuilder can rebuild State from GENESIS_REWARD ledger records
GenesisReward records create deterministic reward CoinLots
State supply audit counts GenesisReward supply
legacy MintRecord remains only for compatibility
```

Recommended commit:

```bash
git commit -m "Add GenesisReward main state flow"
```
