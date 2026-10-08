# ADR 0004: Evidence, unbonding and weak-subjectivity windows

**Status:** accepted for the `nodo/0.5` development protocol. The v1 BFT-time
and light-client checkpoint rules remain separate implementation gates.

## Decision

The development protocol admits equivocation evidence only when its signed
offense height is earlier than its inclusion height and no more than 907200
blocks old (21 validator epochs). This rule is checked both before gossip
admission and when a finalized block executes; a peer's `detectedAt` value
cannot reset the age of an old offense. Duplicate evidence remains invalid.

Unlock and validator exit start unbonding for at least 1209600 blocks (28
validator epochs) **and** 28 days plus a 300-second future-block margin from
the finalized block timestamp. Both deadlines must pass before a withdrawal
can execute. A second unlock of an already unbonding position extends its
whole pending amount to the later deadline. The position stores both
deadlines in canonical state and snapshots. Old domain-codec state is rejected
by the incompatible `nodo/0.5` format.
Mempool admission checks against the latest finalized block timestamp, so a
transaction may enter one block after its exact execution deadline; final
execution checks the candidate block timestamp again.

At every withdrawal, execution examines all selected validator sets that
could still support admissible evidence. It retains at least the maximum
historical voting weight for the validator operator in locked stake across
the operator's current keys. Missing historical coverage rejects the
withdrawal. Thus queued exits and the churn cap cannot release collateral
while the old key may still vote or be punished. A key rotation records the
old-to-new staking address in canonical lifecycle state; later evidence for
the old key slashes and jails the successor account.

The weak-subjectivity period for a new node or one returning after a long
absence is **14 days**: operators must obtain a trusted recent checkpoint out
of band before accepting a chain after the trust period. Full replay from
genesis audits state transitions but does not by itself resolve competing
long-range histories signed by keys whose stake has already unbonded.
Automated checkpoint-age enforcement,
authenticated light-client transitions and checkpoint distribution belong
to Phase 5.

## Boundaries and production gates

The two height windows give a deterministic seven-epoch gap between the last
admissible evidence and earliest withdrawal. The calendar deadline prevents
fast blocks from compressing unbonding; the extra 300 seconds compensates for
the present future-block timestamp allowance. Block timestamps are not yet
the v1 BFT time specified in [protocol v1](protocol-v1.md), so the calendar
rule still depends on honest wall-clock bounds. The v1 evidence age must also
be checked against authenticated BFT time, not only height. Censorship of all
evidence through the window, one-to-one genesis stake backing, and automated
weak-subjectivity checkpoint enforcement remain open production risks tracked
in roadmap items 4.9, 3.16 and Phase 5 respectively.
