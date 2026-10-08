# Consensus

Nodo uses a weighted BFT-style consensus foundation with explicit voting phases and quorum certificates.

## Current model

The current design includes:

- validator identities;
- validator weights;
- proposer selection;
- block proposal validation;
- PREVOTE and PRECOMMIT messages;
- quorum certificate creation;
- finalization from valid PRECOMMIT quorum;
- timeout/view-change foundations;
- persistent consensus recovery records;
- slashing evidence for conflicting votes and proposer equivocation foundations.

## Quorum rule

A finalized block requires a quorum certificate formed from valid PRECOMMIT votes representing the configured threshold of validator weight.

The fixed rule in `nodo/0.5` and the [v1 design contract](../spec/protocol-v1.md)
is `floor(2W/3)+1`, where `W` is the positive total voting weight in the
validator-set snapshot for that height. The numerator and denominator fields
must be exactly 2 and 3; they are not a configurable safety knob. A QC with
exactly two thirds of a divisible total is rejected, as is one that reports a
different required weight. See [ADR 0002](../spec/adr-0002-fault-model-and-quorum.md).

The fault assumption is Byzantine weight `B < W/3` at each height. Safety is
required even before network stabilization. Liveness additionally assumes
that honest nodes can communicate within a finite bound after an unknown GST,
timeouts grow beyond that bound, an honest proposer eventually acts, and data
is available. The current lock, recovery and validator-set transition code
still needs the work tracked in the roadmap before those system-wide
properties can be claimed.

## Validator weights

Validator voting power should be derived from the active validator set snapshot for the relevant height/epoch. Stake changes must not unexpectedly rewrite historical voting power.

The documented direction is:

```text
active locked stake → epoch projection → validator-set snapshot → linear raw-unit weight
```

Historical quorum verification must use the validator-set snapshot that was valid for the finalized height.
The [weight decision](../spec/adr-0001-validator-weight.md) requires each
unit of v1 voting weight to be backed by one distinct locked stake unit.
Splitting stake across keys cannot create weight. The `nodo/0.5` development
runtime uses linear weight and epoch projection. The selected set remains
fixed for 43200 voting
heights, and the finalized boundary may change at most 3333 basis points of
the previous set's voting weight. New stake and validator registration wait
two epoch boundaries; excess changes remain staged. Historical lookups use
the set selected for that height. See [ADR 0003](../spec/adr-0003-epoch-validator-sets.md).
One-to-one stake-lot backing and formal dynamic-set safety checks remain open.
The enforced 21-epoch evidence and 28-epoch plus calendar unbonding windows
are defined in [ADR 0004](../spec/adr-0004-accountability-windows.md).

## Remaining consensus work

Before public testnet, implementation and formal checking must establish:

- timeout behavior;
- view-change behavior;
- fork-choice behavior;
- evidence requirements for equivocation;
- how validator-set changes affect consensus safety;
- how a node recovers after restart.

## What Proof of Protection does not replace

Proof of Protection is not the consensus algorithm. Consensus decides finality. Proof of Protection decides how measurable protection work affects rewards, penalties, and audit state.
