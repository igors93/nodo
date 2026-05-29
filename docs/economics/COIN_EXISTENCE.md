# Nodo Coin Existence

Status: Concept Guide  
Version: NODO-COIN-EXISTENCE-V1

## Purpose

This document defines how Nodo should think about coin existence and traceability.

In simple terms:

```text
A coin cannot be spent just because a wallet says it exists.
The blockchain must prove that the coin lot exists and was not already spent.
```

---

## Coin Lot Model

Nodo should use identifiable coin lots.

A coin lot is a group of spendable units with a known origin.

Example:

```text
CoinLot {
  id: 7a91...
  origin: GENESIS_REWARD
  owner: nodo1abc...
  amount: 50 NODO
  status: UNSPENT
}
```

The chain should be able to answer:

```text
Does this coin lot exist?
Who owns it?
How was it created?
Was it already spent?
```

---

## Coin Birth

New coins should be born through accepted chain records.

Recommended record:

```text
GenesisRewardRecord
```

Example:

```text
GenesisRewardRecord {
  epoch: 40
  validator: nodo1abc...
  amount: 12.5 NODO
  reason: NETWORK_PROTECTION
}
```

This reward creates a new coin lot.

The coin lot should reference the reward that created it.

---

## Spending Coin Lots

When a transaction spends a coin lot, Nodo should not simply subtract balance.

It should verify and transform lots.

Example:

```text
Input lot:
CoinLot A = 50 NODO owned by Igor

Transaction:
Igor sends 20 NODO to Ana
fee = 1 NODO

Output lots:
CoinLot B = 20 NODO owned by Ana
CoinLot C = 29 NODO change owned by Igor
CoinLot D = 1 NODO fee pool lot
```

The original input lot becomes spent.

This prevents blind balance accounting.

---

## Existence Verification

Before accepting a transaction, validators must verify:

- input coin lot exists;
- input coin lot is unspent;
- sender owns the coin lot;
- signature is valid;
- amount is valid;
- fee is valid;
- outputs do not create hidden inflation.

Simple rule:

```text
sum(inputs) = sum(outputs) + fee
```

For reward creation:

```text
new coin lots must come from accepted GenesisReward records
```

---

## Coin Lot Registry

Nodo should eventually maintain a rebuildable coin lot registry.

Recommended concept:

```text
CoinLotRegistry
```

It should be derived from blockchain history.

It should not be trusted as an independent database.

Correct model:

```text
Blockchain history -> LedgerRecords -> CoinLotRegistry
```

---

## Traceability

Nodo does not need to track every smallest unit as a separate physical coin.

It can track lots.

This is more practical.

A coin lot should have:

- id;
- origin record;
- owner;
- amount;
- status;
- parent lot references when created by transfer;
- block hash where it was accepted.

This gives the chain enough traceability to prove existence and prevent double spending.

---

## Privacy Direction

If Nodo later supports private transfers, privacy must not hide inflation.

Private systems still need a way to prove:

```text
the spent value exists
the value was not already spent
no hidden extra value was created
```

This may require commitments, nullifiers, range proofs, and zero-knowledge proofs in future versions.

---

## Design Sentence

```text
Nodo treats coins as verifiable coin lots with provable origin, ownership, and spend status.
```
