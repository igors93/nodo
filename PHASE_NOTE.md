# Cycle 8 implementation

This phase implements Cycle 8 in two fronts.

## Front A — runtime block production command using mempool + finalization pipeline

New components:

```text
RuntimeBlockPipelineConfig
RuntimeBlockPipelineResult
RuntimeBlockPipeline
```

The CLI also adds:

```bash
nodo produce-demo-block
```

## Front B — persistent block/manifest update after finalized block

New components:

```text
FinalizedBlockStore
FinalizedBlockStoreResult
```

Nodo can now write finalized block artifacts under `.nodo/blocks/` and update
the runtime manifest after a block is finalized.

Recommended commit:

```bash
git commit -m "Add runtime block pipeline and finalized block persistence"
```
