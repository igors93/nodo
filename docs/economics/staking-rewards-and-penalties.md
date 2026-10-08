# Staking, Rewards and Penalties

Staking, rewards, and penalties are connected but must remain distinct.

## Staking

Stake represents commitment. It should not automatically print rewards by itself.

Correct model:

```text
stake increases commitment and validator eligibility
valid protection work earns reward
bad behavior creates penalty evidence
```

## Weight projection

Stake changes should be recorded in the staking registry, but consensus voting power should change only through an epoch-bound projection.

The [v1 decision](../spec/adr-0001-validator-weight.md) is:

```text
active locked stake → linear raw-unit weight → epoch validator-set snapshot
```

Each eligible weight unit must be backed by one distinct, active, locked stake
unit. Partitioning stake among validator keys cannot increase aggregate power.
The minimum stake limits cheap identities, but does not establish Sybil
resistance by itself. A stake-rich actor can still hold a large fraction of
voting power. Historical quorum verification uses the set snapshot for the
finalized height. The current development runtime has linear weight and
epoch projection in `nodo/0.5`; full v1 stake-lot backing remains roadmap work.

## Accountability windows

In `nodo/0.5`, an unlock or validator exit starts a 28-epoch height hold and
a 28-day timestamp hold with a 300-second future-block margin. Withdrawal
requires both deadlines and must leave enough stake locked to back every
historical vote still within the 21-epoch evidence window. A rotated key
retains its predecessor's slash liability. See
[ADR 0004](../spec/adr-0004-accountability-windows.md) for the exact rules
and the remaining BFT-time and evidence-inclusion work.

## Rewards

Reward settlement should be deterministic and evidence-backed. A reward record should show:

- epoch;
- validator identity;
- eligible work;
- score/weight input;
- reward pool;
- reward amount;
- ledger record;
- state effect.

## Validator score

Validator score should be a deterministic output of protocol evidence, not a subjective reputation value.

Score may be affected by:

- valid participation;
- missed duties;
- equivocation evidence;
- invalid proposals;
- network-risk signals;
- slashing or jailing status.

## Penalties and slashing

Penalties require canonical evidence. Examples:

- conflicting votes;
- double proposal;
- invalid proposal with evidence;
- repeated protocol violations;
- finalized slashing evidence.

Penalty application must be idempotent. The same evidence must not punish twice.

## Open work

Before public testnet, the project must finalize:

- minimum active stake;
- unbonding period;
- jailing and unjailing rules;
- reward caps;
- score formula;
- exact penalty amounts;
- evidence retention requirements.
