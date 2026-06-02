# Blocks and Finalization

Nodo's localnet block path is designed to make finality auditable.

## Flow

1. Pending transactions are loaded from the persistent mempool.
2. A candidate block is built.
3. The block is checked structurally.
4. Transaction signatures and admission rules are checked.
5. State transition is previewed without partial mutation.
6. Validator votes are collected.
7. A quorum certificate and finalized record are produced.
8. The finalized artifact is persisted.
9. Runtime reload verifies the artifact before accepting it.

## Finalized Artifacts

Finalized artifacts carry the block plus domain evidence sections, including monetary, treasury, governance, validator, and slashing-related records as the implementation evolves.

## Safety Notes

- Invalid blocks must not receive valid finality.
- Duplicate or unknown validator votes are rejected.
- State roots must match reload and audit.
- Finalized history must remain rebuildable.
