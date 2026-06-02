> Archived document.
> This file is kept for historical context only and may not describe the current Nodo implementation.

# Nodo Fork Choice and Finalized Checkpoints

Status: Cycle 5 Implementation  
Version: NODO-FORK-CHOICE-V1

## Purpose

This phase adds the first fork-choice layer.

The rule is simple:

```text
never adopt a chain that conflicts with local finalized blocks
```

## New Components

```text
FinalizedCheckpoint
ChainForkSummary
ForkChoiceResult
ForkChoicePolicy
```

## Selection Order

Nodo chooses a candidate chain only when it is safe and better:

```text
1. candidate must be valid
2. candidate must contain local finalized checkpoint
3. candidate must not conflict with local finality
4. candidate with higher finality wins
5. otherwise longer valid candidate wins
6. identical latest hash is equal
7. otherwise keep local
```

## Security Rule

Finality is stronger than length.

A longer chain that does not contain the local finalized checkpoint is rejected.
