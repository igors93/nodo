# Blocks and Finalization

Nodo separates the real distributed finalization path from the localnet development helper.

## Flow

1. Pending transactions are loaded from the persistent mempool.
2. A candidate block is built.
3. The block is checked structurally.
4. Transaction signatures and admission rules are checked.
5. State transition is previewed without partial mutation.
6. Distributed validators cast PREVOTE and then PRECOMMIT after prevote quorum.
7. A quorum certificate is assembled from PRECOMMIT votes only.
8. `RuntimeBlockPipeline::commitCertifiedBlock` applies the certified block,
   writes the finalized record and persists the finalized artifact.
9. Runtime reload verifies the artifact before accepting it.

`RuntimeBlockPipeline::produceAndFinalizeLocalnetBlock` is a DEVELOPMENT_LOCAL-only
helper for CLI/tests. It may build one local PRECOMMIT-backed QC in-process, but
it rejects staging and production network classes and is not the protocol path
for distributed consensus.

## Finalized Artifacts

Finalized artifacts carry the block plus domain evidence sections, including monetary, treasury, governance, validator, and slashing-related records as the implementation evolves.

## Safety Notes

- Invalid blocks must not receive valid finality.
- Duplicate or unknown validator votes are rejected.
- QCs must contain only PRECOMMIT votes; legacy approval shortcuts are not valid finality.
- State roots must match reload and audit.
- Finalized history must remain rebuildable.
