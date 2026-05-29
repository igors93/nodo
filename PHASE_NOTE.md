# Protection economics foundation phase

This phase starts implementing the new Nodo economic model.

New code:

```text
ValidationWorkRecord
ValidatorScoreRecord
EpochEmissionPolicy
ProtectionEpoch
GenesisRewardRecord
```

What it does:

```text
records useful validator work
keeps validator score bounded from 0 to 100
calculates controlled epoch emission caps
calculates reward pool from fees + capped security emission
creates traceable reward CoinLots from GenesisReward records
```

Nothing is deleted yet.

The old demo MintRecord remains until the new model is integrated safely into blocks, state rebuild, serialization and storage.

Recommended commit:

```bash
git commit -m "Add protection economics foundation"
```
