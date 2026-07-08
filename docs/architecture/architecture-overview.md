# Architecture Overview

Nodo is organized as a layered blockchain runtime. Each layer should expose deterministic behavior and avoid hidden shortcuts.

## High-level flow

```text
CLI / RPC
   ↓
Node runtime
   ↓
P2P / mempool / consensus loop
   ↓
Block proposal and validation
   ↓
State transition engine
   ↓
Finality evidence and persistence
   ↓
Reload / audit / sync
```

## Main architectural principles

1. **Canonical execution before acceptance**  
   Blocks and transactions should be validated by the same deterministic state-transition path that later rebuilds state.

2. **Finalized history before manifest advancement**  
   A finalized block artifact must be written before the manifest moves to the new height/root.

3. **Evidence-backed protocol domains**  
   Governance, treasury, slashing, rewards, and validator penalties should leave replayable records.

4. **Configuration-separated networks**  
   Localnet, testnet, and mainnet should not require separate hidden protocol paths. They should differ by parameters and safety policy.

5. **No silent downgrade**  
   Storage schema, manifest shape, canonical files, and versioned records must reject unsafe or unknown formats.

## Runtime components

- CLI and command policy;
- node data directory manager;
- genesis and network profile registry;
- key store and signing providers;
- mempool and transaction validation;
- consensus loop and vote collection;
- proposer schedule and validator registry;
- state-transition engine;
- finalized block store;
- recovery and reload services;
- P2P transport, gossip, sync, peer policy, and RPC server;
- governance, treasury, staking, reward, and slashing domains.

## Security boundaries

Nodo should treat the following as hard boundaries:

- untrusted network input;
- untrusted persisted files until schema and canonical checks pass;
- untrusted transactions until signature, nonce, fee, and state checks pass;
- untrusted governance outcomes until vote evidence and lifecycle audit pass;
- untrusted treasury execution until policy and governance evidence pass;
- untrusted validator reward/penalty records until replay confirms them.
