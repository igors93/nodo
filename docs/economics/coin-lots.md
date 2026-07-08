# Coin Lots and Ledger

Coin lots are used to make coin existence and ownership traceable.

## Purpose

A coin lot records where a quantity of coins came from and how ownership changes over time. This helps enforce the rule:

```text
no balance without origin
```

## Coin lot lifecycle

```text
genesis/reward/mint/treasury source
      ↓
coin lot birth
      ↓
ownership record
      ↓
transaction input
      ↓
transfer plan
      ↓
new ownership record or spent record
      ↓
ledger and state commitment
```

## Validation goals

Coin-lot validation should prove:

- the lot exists;
- the lot is owned by the spender;
- the lot has not already been spent;
- the transfer amount is valid;
- resulting ownership records are canonical;
- ledger records match the state transition;
- the coin-lot registry digest is included in the state commitment.

## Ledger records

Ledger records provide the canonical economic history. They should cover:

- genesis rewards;
- transfers;
- fees;
- rewards;
- burns;
- treasury execution;
- penalties;
- slashing effects;
- supply reports.

## Audit rule

If account balance and coin-lot history disagree, the state must be rejected.
