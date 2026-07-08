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

Recommended rule:

```text
active locked stake → integer_sqrt(active stake) → consensus weight snapshot
```

This avoids unlimited linear dominance by large validators and protects historical quorum verification.

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
