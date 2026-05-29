# Coin lot transaction integration phase

This phase connects transactions directly to the CoinLotRegistry.

New components:

```text
CoinLotTransactionValidationResult
CoinLotTransferPlan
CoinLotTransactionValidator
```

Modified:

```text
State
```

What this means:

```text
transfer transactions now use registry-backed coin lot validation
locked/spent/slashed lots are rejected
transfer plans explicitly preserve input/output conservation
State keeps the old applyTransferTransaction API but delegates to the new registry path
```

Recommended commit:

```bash
git commit -m "Connect transactions to coin lot registry"
```
