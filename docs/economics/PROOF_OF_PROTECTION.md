# Nodo Proof of Protection

Status: Concept Guide  
Version: NODO-PROOF-OF-PROTECTION-V1

## Purpose

This document defines the main economic and security idea behind Nodo.

In simple terms:

```text
Validators protect the blockchain.
The blockchain records the work.
Coins are created only as controlled rewards for useful protection work.
```

Nodo should not reward validators just because they exist. Nodo should reward validators because they performed useful, verifiable work that helped protect the chain.

---

## Core Idea

Nodo's long-term consensus and reward model should be based on:

```text
Proof of Useful Protection
```

This means the validator earns reward by helping the network do real security work, such as:

- validating transactions;
- verifying that coin lots exist;
- checking that coin lots were not already spent;
- verifying signatures;
- validating blocks;
- responding to integrity challenges;
- serving old blocks or storage proofs;
- helping rebuild state;
- participating honestly in consensus.

---

## What Proof of Protection Is Not

Proof of Protection is not passive income.

A validator should not earn just because:

```text
it has a high score
it exists in the validator set
it owns many coins
it has powerful hardware
it arrived early
```

Those things may help the validator become more trusted or more capable, but reward should come from useful work.

---

## Validator Score vs Work

Nodo separates trust from payment.

```text
validator score = trust signal
validation work = reward reason
```

A high score can help the network decide which validator is more reliable.

But the validator should only earn when it performs accepted work.

Correct model:

```text
high score -> more trust / more selection opportunity
valid work -> reward eligibility
```

Wrong model:

```text
high score -> automatic money forever
```

---

## On-Chain Work Records

Useful work should be recorded in the blockchain.

Recommended record:

```text
ValidationWorkRecord
```

A work record should include:

- validator address;
- epoch number;
- work type;
- target object hash;
- validation result;
- proof or evidence hash;
- timestamp;
- related block hash.

Example:

```text
ValidationWorkRecord {
  validator: nodo1abc...
  epoch: 25
  workType: VERIFY_COIN_EXISTENCE
  targetHash: 81f3...
  result: ACCEPTED
  evidenceHash: 92aa...
}
```

---

## Protection Epoch

Work should be grouped by epochs.

An epoch is a reward period where the network measures useful protection work and calculates reward.

Recommended record:

```text
ProtectionEpoch
```

An epoch should include:

- epoch id;
- start block;
- end block;
- policy version;
- total accepted work;
- fees collected;
- emission cap;
- final reward pool.

---

## Reward Principle

At the end of an epoch:

```text
1. the network counts valid work
2. the network rejects fake or invalid work
3. the network calculates the reward pool
4. validators receive reward according to accepted useful work
```

Reward should be distributed only after the work is accepted by the chain.

---

## Anti-Abuse Principle

More activity should not automatically mean unlimited reward.

Fake work must not generate unlimited coins.

Important rule:

```text
work can compete for the reward pool
work cannot create infinite emission
```

---

## Design Sentence

```text
Nodo rewards validators for useful protection work that can be verified by the blockchain itself.
```
