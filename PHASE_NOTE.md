# Coin lot registry phase

This phase adds the first official coin lot existence registry.

New components:

```text
CoinLotVerificationResult
CoinLotRegistry
CoinLotRegistryRebuilder
```

What this means:

```text
Nodo can verify whether a coin lot exists
Nodo can verify owner and amount
Nodo can reject spent, locked, or slashed lots
Nodo can consume one input lot and create traceable output lots
Nodo can rebuild reward coin lots from GenesisReward history
```

Recommended commit:

```bash
git commit -m "Add coin lot registry"
```
