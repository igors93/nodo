> Archived document.
> This file is kept for historical context only and may not describe the current Nodo implementation.

# Nodo Genesis Config and Network Parameters

Status: Cycle 6 Implementation  
Version: NODO-GENESIS-CONFIG-V1

## Purpose

This phase introduces deterministic network parameters and genesis config.

Before this phase, tests could create genesis blocks directly in code. Now Nodo
has a structured configuration object for creating the first chain state.

## New Components

```text
NetworkParameters
BootstrapValidatorConfig
GenesisConfig
GenesisBuilder
GenesisBuildResult
```

## Security Rules

Genesis config is valid only when:

```text
network parameters are valid
genesis timestamp is positive
genesis memo is safe
bootstrap validator count reaches the configured minimum
each bootstrap validator has a valid public key
each bootstrap validator has positive weight
no bootstrap validator address is duplicated
```

## Important Design Choice

Genesis does not mint arbitrary initial coins.

Bootstrap validators are registered so the chain can begin protection, but coin
creation remains governed by reward/economics rules.
