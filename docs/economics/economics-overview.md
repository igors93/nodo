# Economics Overview

Nodo economics are designed around auditability. Monetary changes should never appear as unexplained balance changes.

## Economic domains

- genesis allocations;
- coin lots;
- ledger records;
- transaction fees;
- burns;
- treasury balances;
- staking and lock records;
- reward settlement;
- validator score;
- slashing and penalties;
- supply reports.

## Core rules

| Rule | Meaning |
| --- | --- |
| No balance without origin | Every balance must be traceable. |
| No inflation without authorization | New coins must be created only by explicit policy. |
| No reward without measurable protection work | Rewards must be tied to auditable work. |
| No penalty without evidence | Punishments must be deterministic and evidence-backed. |
| No treasury spend without policy | Treasury movements require policy and governance evidence. |

## Current maturity

The repository contains economic foundations, but the final public-network parameters are not locked. Before public testnet, the project should define:

- initial testnet supply;
- epoch length;
- inflation cap;
- reward cap;
- fee policy;
- minimum stake;
- unbonding period;
- penalty amounts;
- score effect rules;
- treasury limits.
