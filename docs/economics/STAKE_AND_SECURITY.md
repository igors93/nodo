# Nodo Stake and Security

Status: Concept Guide  
Version: NODO-STAKE-AND-SECURITY-V1

## Purpose

This document defines how locked coins and infrastructure can help secure Nodo.

In simple terms:

```text
Validators can invest in network protection.
But money and hardware should not buy unlimited control.
```

---

## Why Stake Matters

A validator that locks coins has something to lose.

This can increase trust because bad behavior can be penalized.

Recommended record:

```text
StakeLockRecord
```

It should include:

- owner address;
- validator address;
- amount locked;
- start epoch;
- unlock epoch;
- lock duration;
- penalty rules;
- status.

---

## Stake Is Commitment, Not Unlimited Power

Stake should help validator trust.

But it must not create unlimited dominance.

Bad model:

```text
2x stake = 2x control forever
```

Better model:

```text
more stake helps,
but each extra coin gives less additional influence
```

Protocol rule:

```text
StakeWeight = sqrt(locked amount)
```

In simple terms:

```text
locking more coins helps,
but rich validators cannot buy the whole network easily
```

## Epoch Weight Activation

`StakingRegistry` is the source of truth for active locked stake. Deposits,
top-ups, unlock requests and withdrawals are recorded there as canonical state,
but ordinary stake changes do not alter proposer or voting power in the middle
of an epoch.

At each validator-epoch boundary, `ValidatorStakeWeightUpdater` copies active
stake into `ValidatorRegistry` on a temporary projection and commits the whole
projection only if it is valid. The next-height validator-set snapshot then
freezes those weights for consensus and preserves the prior snapshots for
historical QC verification.

The effective rule is:

```text
ConsensusWeight = integer_sqrt(active locked stake)
```

Validators below the minimum active stake have zero effective weight. They are
not eligible to propose or vote even if their lifecycle registration remains
present for audit and later recovery.

---

## Stake and Reward

Stake should not generate reward alone.

Correct model:

```text
stake increases commitment and trust
valid work earns reward
```

Wrong model:

```text
stake alone prints coins forever
```

A validator with stake still needs to perform useful protection work.

---

## Infrastructure Contribution

Nodo may allow validators to invest in useful infrastructure.

Useful infrastructure may include:

- uptime;
- bandwidth;
- historical block storage;
- serving old blocks;
- fast validation;
- response to integrity challenges;
- redundant node operation;
- availability proofs.

Infrastructure should help only when it performs useful work for the network.

---

## Avoid Hardware Domination

Nodo should avoid becoming pure mining under another name.

Bad model:

```text
whoever buys the most hardware wins
```

Better model:

```text
hardware helps perform useful tasks
useful tasks are verified
reward remains limited by epoch policy
```

This lets people invest in protection while reducing wasteful competition.

---

## Slashing and Penalties

Future versions should define penalties for:

- double-signing;
- conflicting block votes;
- fake work submission;
- repeated invalid validation;
- attempted double spend;
- malicious storage response;
- consensus manipulation.

Penalties may include:

- score reduction;
- temporary validator suspension;
- reward loss;
- stake slashing.

---

## Design Sentence

```text
Stake and infrastructure should strengthen Nodo by increasing useful protection capacity without allowing wealth or hardware to buy unlimited control.
```
