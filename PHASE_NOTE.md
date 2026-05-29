# Protection state rebuilder phase

This phase adds a rebuildable state for Nodo's protection economy.

New components:

```text
ProtectionEconomicsState
ProtectionEconomicsRebuilder
```

What this means:

```text
Nodo can now read blocks
find protection economics ledger records
rebuild validator accepted work
rebuild validator latest score
rebuild epoch reward totals
rebuild GenesisReward coin lots
```

Recommended commit:

```bash
git commit -m "Add protection economics state rebuilder"
```
