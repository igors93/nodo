# Explicit transaction CoinLot inputs phase

This phase makes transaction input intent explicit.

New behavior:

```text
TRANSFER transactions can declare input CoinLot ids
transaction id commits to those input ids
validators must spend only declared input lots
declared input lots must cover amount + fee
legacy automatic-input transactions still work
```

Security goal:

```text
prevent silent input substitution
make transaction intent auditable
prepare the project for UTXO-style consensus validation
```

Recommended commit:

```bash
git commit -m "Add explicit transaction coin lot inputs"
```
