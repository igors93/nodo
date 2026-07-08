# Monetary Policy

Nodo monetary policy should be explicit, bounded, and auditable.

## Required properties

A valid monetary policy must define:

- genesis supply;
- circulating supply calculation;
- maximum emission per epoch;
- fee treatment;
- burn treatment;
- reward pool calculation;
- treasury funding rules;
- emergency limits;
- supply report format.

## Emission principle

New emission must be authorized by policy and recorded in canonical ledger records.

Recommended structure:

```text
reward_pool = collected_fees + allowed_security_emission
allowed_security_emission <= epoch_emission_cap
```

## Supply audit

A node should be able to compute supply from canonical records:

```text
genesis supply
+ minted/emitted coins
- burned coins
- slashed/burned amounts
± treasury/account movements
= current supply and balances
```

## Open work

The final public-network policy still needs:

- exact testnet parameters;
- mainnet policy review;
- economics simulation;
- abuse analysis;
- governance controls over parameter changes.
