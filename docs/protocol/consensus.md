# Consensus

Nodo uses a weighted BFT-style consensus foundation with explicit voting phases and quorum certificates.

## Current model

The current design includes:

- validator identities;
- validator weights;
- proposer selection;
- block proposal validation;
- PREVOTE and PRECOMMIT messages;
- quorum certificate creation;
- finalization from valid PRECOMMIT quorum;
- timeout/view-change foundations;
- persistent consensus recovery records;
- slashing evidence for conflicting votes and proposer equivocation foundations.

## Quorum rule

A finalized block requires a quorum certificate formed from valid PRECOMMIT votes representing the configured threshold of validator weight.

The normal target is 2/3+ validator weight, but exact thresholds are network-parameter controlled and must be documented per network profile.

## Validator weights

Validator voting power should be derived from the active validator set snapshot for the relevant height/epoch. Stake changes must not unexpectedly rewrite historical voting power.

The documented direction is:

```text
active locked stake → epoch projection → validator-set snapshot → consensus weight
```

Historical quorum verification must use the validator-set snapshot that was valid for the finalized height.

## Safety assumptions to formalize

Before public testnet, the protocol specification should explicitly document:

- maximum Byzantine validator weight tolerated;
- synchrony or partial-synchrony assumptions;
- timeout behavior;
- view-change behavior;
- fork-choice behavior;
- evidence requirements for equivocation;
- how validator-set changes affect consensus safety;
- how a node recovers after restart.

## What Proof of Protection does not replace

Proof of Protection is not the consensus algorithm. Consensus decides finality. Proof of Protection decides how measurable protection work affects rewards, penalties, and audit state.
