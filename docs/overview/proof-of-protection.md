# Proof of Protection

Proof of Protection is Nodo's security and economics positioning. It treats protection work as measurable protocol behavior rather than an informal validator duty.

## Core Rules

- No inflation without authorization.
- No balance without origin.
- No treasury spend without policy validation.
- No treasury approval without governance evidence.
- No governance decision without verifiable vote evidence.
- No reward without measurable protection work.
- No penalty without verifiable evidence.
- No state accepted if it cannot be rebuilt from history.

## What Protection Means

Protection work can include:

- validating state transitions before finality;
- rejecting malformed or unsafe protocol messages;
- preserving finalized artifacts needed for recovery;
- detecting double votes or invalid validator behavior;
- maintaining peer health and limiting abusive peers;
- rebuilding state during reload and audit;
- producing evidence that a future node can independently verify.

## Why It Matters

A blockchain cannot be safer than the evidence it preserves. Nodo's architecture pushes important decisions through records that can be rebuilt:

- a treasury spend is checked against a proposal, policy, approval, treasury state, and governance lifecycle;
- a governance decision is rebuilt from vote evidence, canonical vote ordering, tally arithmetic, and deterministic decision proof;
- a finalized block is checked against quorum, state-transition preview, storage schema, and reload audit.

Proof of Protection is not a marketing claim that the current code is suitable for production use. It is the rule set guiding the implementation.
