# Nodo Protection State Rebuilder

Status: Implementation Guide  
Version: NODO-PROTECTION-STATE-REBUILDER-V1

## Purpose

This document describes the first rebuildable state for Nodo's protection economy.

In simple terms:

```text
blocks
  ↓
ledger records
  ↓
protection economy state
```

## What This Adds

This phase adds:

```text
ProtectionEconomicsState
ProtectionEconomicsRebuilder
```

The rebuilder reads accepted blockchain history and reconstructs:

```text
accepted validator work
latest validator scores
total security emission
total reward pool
GenesisReward coin lots
```

## Why This Matters

Nodo's rule is:

```text
do not trust saved state
rebuild state from accepted chain history
```

Before this phase, protection records could enter blocks.

After this phase, Nodo can read those records back and produce a protection economy view.

## What Counts

The rebuilder counts:

```text
VALIDATION_WORK records only when result=ACCEPTED
VALIDATOR_SCORE records as latest validator trust
PROTECTION_EPOCH records as security emission and reward pool totals
GENESIS_REWARD records as reward CoinLots
```

Rejected work records are preserved in the ledger, but they do not increase accepted work totals.

## What This Phase Did Not Do

This phase only created the rebuildable state boundary. The larger pieces it
deliberately left out were implemented in later phases:

```text
distribute rewards automatically   -> EpochRewardSettlementService
validate consensus                 -> ConsensusEventLoop + QuorumCertificate
slash stake                        -> CanonicalSlashingTransition
select validators                  -> ProposerSchedule + ValidatorRegistry
connect peer-to-peer networking    -> TcpTransport + GossipMesh
```

## New Test

```text
tests/economics/ProtectionStateRebuilderTests.cpp
```

The test confirms that Nodo can rebuild protection economy state from a blockchain containing protection records.
