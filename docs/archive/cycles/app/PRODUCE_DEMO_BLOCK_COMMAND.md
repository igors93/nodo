# Nodo produce-demo-block Command

Status: Cycle 8 Implementation  
Version: NODO-CLI-PRODUCE-DEMO-BLOCK-V1

## Purpose

This command exercises the local runtime block pipeline.

```bash
nodo produce-demo-block --data-dir .nodo
```

It creates one development transaction, admits it into the mempool, produces a
candidate block, builds local development votes, creates a quorum certificate,
finalizes the block and persists it.

## Required Before Use

Run:

```bash
nodo init --data-dir .nodo
```

## Output

The command prints:

```text
Block height
Block hash
Transactions finalized
Block file path
Manifest latest height
```

## Important Limitation

This is a local demo pipeline command. It is not full P2P block production yet.
