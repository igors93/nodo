# Nodo pruning modes

Nodo supports a durable pruning policy with three modes:

- `ARCHIVE`: keeps every finalized artifact and every snapshot. This is the default and is the safest mode for auditors and validators.
- `FULL`: keeps all finalized artifacts, but removes old fast-sync snapshots below the retained boundary. This reduces cache growth without weakening full-chain replay.
- `LIGHT`: keeps only the current finalized boundary and prunes older finalized artifacts after a verified fast-sync snapshot exists at the boundary. This mode is intended for constrained nodes and depends on the fast-sync snapshot foundation.

The pruning system writes `runtime/pruning_manifest.nodo`. The manifest binds the policy to the local chain id, genesis id, latest block hash, latest state root, retention floor, and snapshot boundary digest.

Security rules:

1. Archive mode never removes canonical chain data.
2. Full mode never removes finalized block artifacts; it only prunes old snapshot cache files.
3. Light mode refuses to prune unless a valid fast-sync snapshot exists at the required boundary.
4. Removed block artifacts leave tombstones in `runtime/pruned_blocks/` so operators can audit what was removed.
5. `ChainAuditor` validates the pruning manifest before accepting an audited data directory.

CLI examples:

```bash
nodo node pruning-status --data-dir .nodo
nodo node prune --data-dir .nodo --pruning-mode full --retain-epochs 2
nodo node prune --data-dir .nodo --pruning-mode light
```
