# Nodo Network Risk and Anti-Sybil Protection

Status: Concept Guide  
Version: NODO-NETWORK-RISK-AND-ANTI-SYBIL-V1

## Purpose

This document defines how Nodo should think about validator concentration, fake validators, and network risk.

In simple terms:

```text
A validator may look independent but actually belong to the same actor.
Nodo should make this harder and less profitable.
```

---

## The Sybil Problem

A Sybil attack happens when one person pretends to be many validators.

Example:

```text
one attacker
        ↓
100 fake validators
        ↓
tries to gain rewards or control
```

Nodo must not trust validator count alone.

---

## Same Network Detection

Nodo may detect risk signals such as:

- same IP;
- same network range;
- same ASN/provider;
- same datacenter;
- many validators joining at the same time;
- validators always voting the same way;
- identical behavior patterns;
- suspicious uptime patterns.

But same network must not be treated as absolute proof of attack.

Legitimate cases exist:

- family members;
- universities;
- companies;
- shared internet;
- NAT;
- small communities;
- hosting providers.

---

## Penalty Instead of Automatic Ban

Nodo should not automatically ban validators just because they appear in the same network.

Better rule:

```text
same network = risk signal
risk signal = reduced trust or reduced reward weight
```

Recommended concept:

```text
NetworkClusterPenalty
```

Example:

```text
1 validator in network: no penalty
2 validators: light penalty
3 to 5 validators: medium penalty
6 or more validators: strong penalty
```

---

## Network Diversity Factor

A validator's reward share may use:

```text
NetworkDiversityFactor
```

Example:

```text
unique network: 1.00
small cluster: 0.75
medium cluster: 0.40
large cluster: 0.20
```

Possible reward share:

```text
RewardShare =
ValidWorkWeight × TrustFactor × StakeFactor × NetworkDiversityFactor
```

---

## VPN and Datacenter Limits

This does not fully stop VPNs or datacenters.

An attacker can still try to hide.

But the goal is to make attacks:

```text
more expensive
slower
less profitable
more visible
```

Security should be layered.

Network risk should be combined with:

- validator score;
- stake lock;
- proof of useful work;
- challenge responses;
- conflict detection;
- reward caps;
- slashing rules.

---

## Future Risk Records

Recommended record:

```text
NetworkClusterPenaltyRecord
```

It should include:

- validator address;
- epoch;
- detected risk type;
- penalty factor;
- evidence hash;
- whether it was automatic or governance-reviewed.

---

## Design Sentence

```text
Nodo should not assume every validator is independent; it should reduce the reward and trust weight of suspicious validator clusters without unfair automatic bans.
```
