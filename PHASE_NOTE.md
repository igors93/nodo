# Cycle 4 implementation

This phase implements Cycle 4 in two fronts.

## Front A — block finalization with quorum certificate

New components:

```text
FinalizedBlockRecord
BlockFinalizationRegistry
BlockFinalizationResult
BlockFinalizer
```

A block can now be appended/finalized only after a valid `QuorumCertificate`
proves enough registered validators approved that exact block.

## Front B — block producer pipeline from mempool

New components:

```text
BlockProductionConfig
BlockProductionPlan
BlockProductionResult
MempoolBlockProducer
```

Nodo can now produce a candidate block from admitted mempool transactions without
mutating the mempool.

Recommended commit:

```bash
git commit -m "Add block finalization and mempool block producer"
```
