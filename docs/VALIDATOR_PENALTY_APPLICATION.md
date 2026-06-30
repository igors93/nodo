# Validator Penalty Application

This phase connects accepted slashing evidence to deterministic validator penalty decisions.

It intentionally does not mutate balances directly inside consensus helpers. Instead it creates a conservative, auditable boundary:

1. slashing evidence is already verified and durable;
2. a penalty policy maps evidence to consequences;
3. a deterministic penalty decision is created;
4. the decision is applied once to an in-memory ledger;
5. the decision can be persisted and announced to peers.

## Added modules

- `consensus::ValidatorPenaltyPolicy`
- `consensus::ValidatorPenaltyDecision`
- `consensus::ValidatorPenaltyLedger`
- `storage::ValidatorPenaltyStore`
- `node::ValidatorPenaltyAnnouncement`
- `node::ValidatorPenaltyRequest`

## Current conservative testnet policy

- warning evidence becomes a warning only;
- slashable double-vote evidence creates a slash decision and jail window;
- slashable invalid-signature evidence creates a jail decision;
- slashable equivocation evidence creates a tombstone decision when policy enables it.

## Security rules

- invalid evidence is rejected;
- invalid policy is rejected;
- each evidence id can produce only one applied penalty decision;
- penalty ids are deterministic from evidence id, validator, action and penalty amount;
- duplicate penalty application is idempotent;
- stored decisions must deserialize back into valid decisions.

## Canonical protocol effects

Verified slashing evidence included in a finalized block now produces one deterministic
`ValidatorPenaltyDecision` and applies the same consequence to all protocol domains
in the canonical state transition:

- `ValidatorPenaltyLedger` records the decision once per evidence id;
- `ValidatorRegistry` updates consensus stake and jail/tombstone status;
- `StakingRegistry` applies the slashed total to stake positions and mirrors jail or tombstone state;
- the resulting slashing, validator and staking domains are included in the protocol state root and replayed during reload.

The transition remains conservative: evidence must already verify against the
historical validator set, duplicated evidence is rejected, and slash amounts are
bounded by bonded stake. Governance appeals and production-grade economics policy
remain future protocol work.

## Finalized sync requirement

Penalty application is now audited on all finalized-state entry points. Local finalization, block-sync import, local artifact import and reload all reject a finalized block if any included slashing evidence lacks its deterministic penalty decision or if the validator registry/staking registry do not reflect the finalized jail, tombstone or bounded slash effect.
