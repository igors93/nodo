# Nodo Protection Ledger Record Integration

Status: Implementation Guide  
Version: NODO-PROTECTION-LEDGER-RECORD-INTEGRATION-V1

## Purpose

This document describes the step where Nodo's protection economics records enter the official ledger pipeline.

In simple terms:

```text
protection work
validator score
protection epoch
GenesisReward
        ↓
LedgerRecord
        ↓
Block
        ↓
Blockchain history
```

## Why This Matters

Before this step, the new protection economics objects existed as standalone code.

After this step, they can become official accepted ledger records.

That means the blockchain can start recording:

```text
who worked
how validator trust changed
what happened in an epoch
which reward created new coin lots
```

## New Ledger Record Types

This step adds:

```text
VALIDATION_WORK
VALIDATOR_SCORE
PROTECTION_EPOCH
GENESIS_REWARD
```

## Security Rule

A record must not enter the blockchain silently.

Each important event must become:

```text
deterministic
hashable
auditable
round-trip serializable
```

## Codec Boundary

`LedgerRecordCodec` now recognizes the new protection record types and checks that the payload prefix matches the record type.

Example:

```text
GENESIS_REWARD must contain GenesisRewardRecord{...}
VALIDATION_WORK must contain ValidationWorkRecord{...}
```

This helps prevent a record from pretending to be one type while carrying another payload.

## Current Limitation

This step does not yet rebuild validator score or reward state from blocks.

That comes later.

This step only makes the records capable of entering blocks safely.

## New Test

```text
tests/economics/ProtectionLedgerIntegrationTests.cpp
```

The test checks that:

- validation work can become a ledger record;
- validator score can become a ledger record;
- protection epoch can become a ledger record;
- GenesisReward can become a ledger record;
- these records can enter a block;
- the codec rejects mismatched protection payloads.
