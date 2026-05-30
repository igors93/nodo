# Cycle 9 implementation

This phase implements Cycle 9 in two fronts.

## Front A — load persisted blocks and rebuild runtime from data directory

New components:

```text
RuntimeStateLoader
RuntimeStateLoadResult
FinalizedBlockFileCodec
LedgerRecord::fromPersistedFields
```

Nodo can now rebuild runtime state from `.nodo/manifest.nodo` and
`.nodo/blocks/block_<height>.nodo`.

## Front B — persistent mempool and transaction submit CLI command

New components:

```text
PersistentMempoolStore
PersistentMempoolWriteResult
PersistentMempoolLoadResult
```

The CLI also adds:

```bash
nodo submit-demo-transaction
```

Recommended commit:

```bash
git commit -m "Add runtime reload and persistent mempool"
```
