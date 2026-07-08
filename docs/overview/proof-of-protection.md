# Proof of Protection

Proof of Protection is the guiding economic and audit model of Nodo. It is not a replacement name for the consensus engine.

## Definition

Proof of Protection means that validators and protocol participants should be rewarded only for protection work that is measurable, useful, and verifiable by the network.

Protection work may include:

- producing valid blocks;
- voting correctly in consensus;
- maintaining finality evidence;
- relaying valid protocol data;
- preserving auditable state;
- participating in governance evidence;
- helping the network remain available and recoverable.

## Separation from consensus

Nodo should be documented and reviewed as layered protocol design:

```text
Consensus
  Determines how blocks become finalized.

State transition
  Determines how transactions and protocol records change state.

Proof of Protection
  Determines how measurable protection work affects rewards, score, penalties, and economics.

Audit and rebuild
  Determines whether another node can reproduce accepted state later.
```

This separation matters. A consensus protocol can finalize a block, while Proof of Protection determines whether the validators involved earned rewards, lost score, or produced penalty evidence.

## Core principles

| Principle | Meaning |
| --- | --- |
| No inflation without authorization | Monetary expansion must be explicit, bounded, and auditable. |
| No balance without origin | Every balance must trace back to genesis, mint, transfer, reward, fee, burn, treasury, or penalty history. |
| No reward without measurable protection work | Rewards should be tied to protocol work that can be verified later. |
| No penalty without evidence | Slashing and penalties must require canonical evidence and must be idempotent. |
| No treasury spend without policy validation | Treasury actions must satisfy policy limits, approval rules, balance checks, and execution rules. |
| No governance decision without vote evidence | Proposal outcome must be rebuilt from recorded votes and tally rules. |
| No accepted state without rebuild | Node state must be reproducible from canonical history. |

## What must be avoided

Proof of Protection should not become:

- a vague reputation system;
- a subjective validator ranking;
- a reward mechanism based only on stake;
- a way to pay validators for creating artificial work;
- a hidden shortcut around consensus or state-transition rules.

## Implementation direction

The current implementation contains foundations for validator score, stake, reward records, coin lots, penalty evidence, governance audit, treasury evidence, and state rebuilding. The final Proof-of-Protection model still needs testnet parameters and formal specification before public value can depend on it.
