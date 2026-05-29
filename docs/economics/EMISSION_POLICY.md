# Nodo Emission Policy

Status: Concept Guide  
Version: NODO-EMISSION-POLICY-V1

## Purpose

This document defines how Nodo should think about coin creation.

In simple terms:

```text
Nodo should not create infinite coins.
Nodo also does not need to start with a fixed premine.
Coins should be created by controlled economic rules.
```

---

## Core Principle

Nodo should move toward this model:

```text
initial spendable supply = 0
        ↓
validators protect the network
        ↓
epochs calculate allowed rewards
        ↓
GenesisReward records create new coin lots
```

This replaces the idea of giving a large fixed amount of coins at the beginning.

---

## Dynamic but Limited Emission

Nodo's emission should not be fixed forever per epoch.

It can be dynamic.

But it must always follow a hard rule:

```text
each epoch has a maximum amount of new coins that can be created
```

The amount of new coins can depend on:

- current circulating supply;
- target inflation;
- epoch duration;
- useful network work;
- security demand;
- fees collected.

But it must not be unlimited.

---

## Inflation Target

Nodo may use a target inflation rate.

Example:

```text
target yearly inflation = 4%
```

This number is only an example and can change later.

A simple emission cap formula:

```text
NewEmissionCap =
current circulating supply × target yearly inflation / epochs per year
```

Example:

```text
current circulating supply: 1,000,000 NODO
target yearly inflation: 4%
epochs per year: 365

NewEmissionCap per epoch ≈ 109.58 NODO
```

This means the epoch cannot create more than 109.58 new NODO.

---

## Fees and Security Emission

Validator rewards should have two sources:

```text
1. fees already paid by users
2. new security emission allowed by policy
```

Recommended formula:

```text
RewardPool = FeesCollected + SecurityEmission
```

Where:

```text
SecurityEmission <= NewEmissionCap
```

This means:

- user fees are recycled to validators;
- new coins are created only inside the policy cap;
- validators are paid for security work;
- inflation remains controlled.

---

## Work Demand Factor

Useful work can decide how much of the emission cap is used.

Example:

```text
NewEmissionCap = 100 NODO

low useful work:
SecurityEmission = 30 NODO

medium useful work:
SecurityEmission = 60 NODO

high useful work:
SecurityEmission = 100 NODO
```

Important rule:

```text
work can decide how much of the cap is used
work cannot increase the cap without policy rules
```

---

## GenesisReward

New coins should be created through a clear record:

```text
GenesisRewardRecord
```

A `GenesisRewardRecord` should include:

- epoch id;
- validator address;
- reward amount;
- reward reason;
- work summary hash;
- emission policy version;
- block hash where the reward was accepted.

This gives every newly created reward coin a clear origin.

---

## Inflation Safety Rules

Nodo should follow these rules:

```text
do not create coins without an accepted record
do not let work create unlimited emission
do not let score create passive emission
do not let validators decide their own reward
do not let fees automatically increase new emission
do not hide coin creation inside privacy systems
```

---

## Design Sentence

```text
Nodo creates new coins only through transparent epoch rules that reward useful network protection while keeping inflation controlled.
```
