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

## Protection pillars

Every reward must trace to measurable work in exactly one pillar
(`economics::ProtectionWorkType`):

```text
Proof of Protection
├── Consensus protection      proposing finalized blocks
├── Finality participation    PRECOMMIT votes inside quorum certificates
├── Network availability      relaying and serving data (not yet provable)
├── Data availability         integrity and availability challenges
├── Historical archival       Proof of Archival (ADR 0014)
└── Useful compute            reserved for a future protocol version
```

The epoch emission cap from `EpochEmissionPolicy` is the ceiling of the whole
Epoch Protection Budget (`economics::ProtectionBudgetSplit`). Consensus keeps
the remainder and at least 60%; a pillar without verifiable work receives
nothing, and unearned budget is never minted.

## Historical preservation is protection work

The intended archival protocol lets normal nodes keep bounded history while
archive nodes preserve it. Once the outstanding consensus integration and
protocol upgrade gates are complete, the network can pay providers for
verified preservation ([ADR 0014](../spec/adr-0014-bounded-storage-and-proof-of-archival.md)):

- providers bond per segment slot and are assigned segments by the network;
- unpredictable challenges, seeded by finalized blocks, sample pieces of a
  segment; the provider answers with the pieces and Merkle proofs;
- verification is cheap, while answering without the data fails with high
  probability;
- rewards scale with bytes preserved, availability, reliability and the
  scarcity of the segment, inside the archival share of the budget;
- a missed challenge lowers score and reward; only a signed invalid answer
  is evidence that can justify a penalty.

Proof of Archival protects history. It does not finalize anything and never
replaces the BFT consensus or its quorum certificates.

## Implementation direction

The current implementation contains foundations for validator score, stake, reward records, coin lots, penalty evidence, governance audit, treasury evidence, and state rebuilding, plus the Proof-of-Archival reference (challenges, proofs, scoring, replication, bounded rewards). Archival rewards are not minted until a protocol upgrade activates them (roadmap 6.9). The final Proof-of-Protection model still needs testnet parameters and formal specification before public value can depend on it.
