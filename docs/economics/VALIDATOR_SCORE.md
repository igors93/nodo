# Nodo Validator Score

Status: Concept Guide  
Version: NODO-VALIDATOR-SCORE-V1

## Purpose

This document defines how validator score should work in Nodo.

In simple terms:

```text
score is trust
score is not money
```

A validator with a high score is more trusted by the network, but it should not receive rewards automatically without doing useful work.

---

## Score Range

Nodo validator score should be limited:

```text
0 to 100
```

Example meaning:

```text
0   = not trusted
50  = normal trust
100 = very trusted
```

A score above 100 should not exist.

A score below 0 should not exist.

---

## On-Chain Score

Validator score should be recorded on-chain.

Recommended record:

```text
ValidatorScoreRecord
```

It should include:

- validator address;
- epoch;
- previous score;
- new score;
- score change reason;
- evidence hash;
- related block hash.

Example:

```text
ValidatorScoreRecord {
  validator: nodo1abc...
  epoch: 10
  previousScore: 72
  newScore: 76
  reason: CONSISTENT_VALIDATION
  evidenceHash: abcd...
}
```

This allows every node to verify how the score changed.

---

## What Raises Score

Score may increase when a validator:

- validates correctly;
- stays online;
- responds to network challenges;
- does not sign conflicting data;
- helps maintain chain integrity;
- serves historical data;
- participates honestly over time;
- avoids suspicious cluster behavior.

Score should increase slowly.

This avoids fast reputation farming.

---

## What Lowers Score

Score may decrease when a validator:

- validates incorrectly;
- goes offline too often;
- fails integrity challenges;
- signs conflicting blocks;
- submits fake work;
- repeats invalid behavior;
- appears in suspicious validator clusters;
- tries to double-sign or manipulate consensus.

Score should fall faster than it rises.

This protects the network.

---

## Score and Reward

Score should influence trust and selection.

It should not be direct salary.

Correct model:

```text
score helps the network decide who is reliable
work records prove who did useful work
rewards are paid for accepted useful work
```

Wrong model:

```text
score 100 = automatic reward forever
```

---

## Score as Trust Factor

In reward calculation, score can act as a trust factor.

Example:

```text
TrustFactor = score / 100
```

Then a validator with score 80 has a trust factor of 0.80.

But the validator still needs accepted work.

Possible reward share model:

```text
RewardShare =
ValidWorkWeight × TrustFactor × StakeFactor × NetworkDiversityFactor
```

---

## Score Update Philosophy

Nodo should follow these rules:

```text
score is earned slowly
score can be lost quickly
score must be explainable
score changes must be recorded
score must be rebuildable from chain history
score must not be hidden off-chain
```

---

## Design Sentence

```text
Validator score is an on-chain trust signal that helps Nodo choose reliable protectors without becoming passive income.
```
