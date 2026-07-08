# Project Overview

Nodo is a security-first blockchain protocol foundation written in C++20. Its core design goal is to make important protocol activity measurable, auditable, and rebuildable.

## What Nodo is

Nodo is an experimental blockchain foundation for:

- deterministic state transition;
- verifiable finality;
- evidence-backed validator accountability;
- auditable monetary changes;
- treasury execution with policy and governance evidence;
- rebuildable state from canonical history;
- localnet and testnet-candidate operation.

The project is currently best understood as a protocol and runtime foundation, not a finished production network.

## What Nodo is not yet

Nodo is not yet:

- a production mainnet;
- a finished public blockchain economy;
- a wallet/custody product;
- a legally reviewed treasury system;
- a permissionless production validator network;
- a finalized Proof-of-Protection economic system.

## Design goals

Nodo is built around the following goals:

| Goal | Meaning |
| --- | --- |
| Rebuildable state | A node should be able to rebuild accepted state from genesis and finalized history. |
| Verifiable finality | Finalized blocks must carry enough evidence to verify quorum and validity. |
| Auditable economics | New coins, rewards, burns, fees, penalties, and treasury movements must leave canonical records. |
| Evidence-backed governance | Governance decisions must be reproduced from proposal, vote, tally, decision, and execution records. |
| Accountable validators | Validator rewards and penalties must be tied to deterministic records. |
| Safe network evolution | Localnet, testnet, and mainnet must differ by configuration and safety policy, not hidden protocol shortcuts. |

## The central rule

Nodo should never accept important protocol state merely because it appears in a database. The state must be explainable by canonical history.

In simple terms:

```text
history + deterministic rules = accepted state
```

If the state cannot be rebuilt, the node should reject it instead of silently trusting it.
