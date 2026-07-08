# Sync, Pruning and Snapshots

Sync, pruning, and snapshots must preserve the rebuildability principle.

## Sync rule

A node may download data from peers, but it must verify finality evidence, canonical serialization, and state commitments before accepting that data.

## Fast sync

Fast sync should be tied to finalized checkpoints and quorum evidence. It must not accept an arbitrary state snapshot without a verifiable commitment path.

## Snapshots

Snapshots should include:

- source height;
- state root;
- protocol-domain roots;
- validator-set context;
- storage format version;
- hash/signature evidence when applicable;
- compatibility metadata.

## Pruning

Pruning should remove old data only when the node still retains enough information for its declared audit mode.

Possible audit modes:

- full history;
- checkpoint plus proofs;
- recent history plus finalized snapshots;
- light-client proof mode.

## Open work

Before public testnet, the project should document exactly which data a pruned node may delete and which proof obligations remain mandatory.
