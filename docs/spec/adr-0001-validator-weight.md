# ADR 0001: Stake-proportional validator voting weight

**Status:** accepted for the v1 design contract; the development runtime uses
this weight function since `nodo/0.2` (and retains it in `nodo/0.5`). The remaining v1 state and consensus
rules are tracked separately in the roadmap.

## Threat and decision

An adversary can create any number of validator keys, divide its locked stake
among them, and coordinate all votes. Key count is therefore not an economic
resource. A safe weight rule must preserve the adversary's aggregate voting
weight under every partition of its stake, subject to one stake unit backing
at most one validator at a time.

For eligible validator `i`, set `weight_i = locked_active_stake_i` in raw
units (1 NODO = 100000000). Set `W = sum(weight_i)` over the active set,
using checked arithmetic. Reject any state for which an individual stake or
the aggregate cannot be represented. The quorum and proposer algorithms use
these exact weights from the validator-set snapshot at the relevant height.
Stake must be authenticated by the canonical state transition, be locked and
slashable, and be allocated exactly once. Genesis stake has the same backing
requirement. Entry requires at least 0.01 NODO in the current development
runtime; v1 commits its minimum in genesis. Increasing the minimum only
increases the cost of extra identities; it does not substitute for the linear
rule.

For a partition `S = s_1 + ... + s_k`, the attacker's total is exactly
`sum(weight(s_i)) = S` for eligible parts. Parts below the minimum contribute
zero, so splitting cannot increase voting power. If the honest set owns `H`
active units, the attacker's fraction is at most `S/(S+H)`. The familiar
less-than-one-third Byzantine assumption is thus stated in **locked stake
weight**, not validator count. A party with enough stake can still dominate;
the protocol does not promise a wealth-distribution property.

## Rejected alternatives

- `floor(sqrt(stake))`, logarithms, equal votes per key, and fixed or soft
  per-validator weight caps reward splitting. With `m = 1,000,000` raw units,
  one key holding `100m` has square-root weight 10,000, while 100 keys holding
  `m` each have aggregate weight 100,000.
- A naive top-K active-set cap also rewards splitting even with linear
  weights. With two seats, 100 attacker units and one honest unit give the
  attacker 100/101 of voting weight. Dividing the 100 into two 50-unit keys
  displaces the honest key and yields 100% of selected weight. V1 therefore
  has no top-K cap. An active-set bound is a separate resource-control design
  problem, not a shortcut in this weight decision.
- A higher minimum stake alone does not prevent a well-funded attacker from
  creating many identities. Identity attestation would add a trusted
  admission authority and would not prove unique economic ownership.

## Consequences and verification

The current development registry uses one weight per stored stake raw unit,
requires 0.01 NODO per eligible validator, rejects aggregate `u64` overflow,
and rejects individual stake above the signed amount range. Registration,
activation, unjailing, stake update and key rotation reject and roll back
invalid updates. Protocol-version bumps separate peers and stored manifests;
there is no in-place migration. Regression tests
partition stake across keys, compare quorum thresholds and historical weight
snapshots, and test the minimum and overflow boundary.

This decision does not close the separate work on epoch-only validator-set
transitions, stake-lot backing in the v1 state machine, strict quorum
thresholds, proposer fairness, anti-DoS limits for unbounded active sets, or
genesis-accounting migration. Those rules are required before production.
