# ADR 0009: Deterministic v1 resource units and congestion fee

**Status:** accepted Phase 1 decision. `V1ResourceFee` is a checked executable
reference with boundary and overflow tests. The `nodo/0.7` runtime still uses
absolute fees, fee splitting and producer-only size policy; Phase 3.10 and
3.12 must implement this contract in all v1 execution paths before activation.

## Decision and units

V1 has one nonnegative, integer resource unit schedule. It is a consensus
accounting measure, not elapsed CPU time. The schedule below is fixed for v1;
changing any coefficient, type surcharge or limit requires a new protocol
version and activation height. All operands are unsigned and arithmetic is
checked; the fixed, bounded meter terms fit u64, while fee and adjustment
products use 128-bit intermediates. Overflow invalidates the candidate; it
never wraps, saturates an offered fee or becomes zero.

A valid transaction is executed hypothetically before fee admission. Its
canonical, complete top-level bytes and resulting canonical, complete receipt
bytes determine the fee. Let `I` be the number of input lots, `O` the number
of newly created coin lots, and `E` the number of receipt effect IDs. The
offered `fee` is already a transaction field, so it determines change and
these counts without a fee fixed point. Invalid execution invalidates the
transaction before any fee is charged.

```text
tx_units = transaction_bytes + receipt_bytes + 1024
         + 128*I + 256*O + 64*E + type_surcharge[type]
```

The 1024 units reserve one Ed25519 verification. Input and output charges
cover coin-lot lookup and creation; effect IDs charge committed state changes.
The fixed type surcharge covers the type's bounded protocol work. These are
protocol prices, not implementation-dependent counters, so an optimizer or
cache cannot change consensus units. The registry is closed under
[ADR 0011](adr-0011-fixed-function-v1.md): there is no v1 VM instruction
meter, contract call or unpriced code path. The exact schedule is:

| Type | Surcharge |
| --- | ---: |
| 1 TRANSFER | 256 |
| 2 BURN | 128 |
| 3 STAKE_DEPOSIT, 4 STAKE_UNLOCK, 5 STAKE_WITHDRAW, 6 STAKE_TOP_UP | 512 |
| 7 VALIDATOR_REGISTER | 1024 |
| 8 VALIDATOR_EXIT_REQUEST, 9 VALIDATOR_UNJAIL_REQUEST | 384 |
| 10 VALIDATOR_KEY_ROTATE, 11 GOVERNANCE_PROPOSE | 768 |
| 12 GOVERNANCE_VOTE | 512 |
| 13 GOVERNANCE_EXECUTE | 1536 |

`I <= 128`, `O <= 2` and `E <= 256` are independent v1 validity limits.
The receipt schema makes its complete size exactly `92 + 32*E` bytes; a
different declared length is invalid. The full transaction and receipt obey
their separate binary size caps. No unbounded iteration over user-provided
lists is permitted during execution.
Validators re-execute each transaction and recompute the receipt, counts,
units and fee; a proposer-supplied receipt or header is not authoritative.
The transaction receipt records its computed `units` and offered `fee`.

Evidence and system transitions are also charged against the block, even
though they carry no transaction fee. The
[proposer-priority transition](adr-0012-proposer-selection.md) emits one
mandatory, bounded tag-9 system record and receipt at every finalized height;
the [liveness-window transition](adr-0013-liveness-accountability.md) emits
one mandatory tag-10 record and receipt. Their units are reserved before
user transaction selection:

```text
evidence_units = complete_evidence_bytes + 2*1024 + 512
system_units   = nested_record_bytes + complete_receipt_bytes
               + 1024 + 64*receipt_effect_ids
liveness_units = system_units + 128*max(old_set_count, next_set_count)
```

`nested_record_bytes` includes its four-byte length prefix. Evidence verifies
two conflicting signatures, and its slashing system record is charged
separately. The liveness surcharge pays for re-encoding and checking every
active counter, including nonsigners; it is never multiplied only by QC
signers. At non-boundary heights the two set counts are equal; a boundary
charges the larger set because both the old assessment and new vector reset
must be processed. A system receipt has `fee = 0`, its computed `units`, and at most
2045 effect IDs under the 65536-byte receipt cap. A block contains at most
32 evidence objects, 4096 system records and 4096
transactions. Any state transition requiring more work must be split into
deterministic, replayable bounded steps; optional transactions and evidence
cannot consume the budget reserved for already mandatory transitions.

The header's `resource_units` equals the checked sum of all transaction,
evidence and system-record units, not a proposer estimate. The complete
top-level body must fit `max_block_bytes`; every transaction must fit both
`max_tx_bytes` and `max_tx_units`. User transactions together may consume at
most `floor(3*max_block_units/4)`, leaving a quarter available for evidence
and system work. The total may not exceed `max_block_units`. The full
finalized artifact and all nested objects must also obey ADR 0008 limits.
Producer, voter, finalizer, import, replay, snapshot and sync validation must
enforce the **same** historical parameter set and recomputed limits.

## Fee price and parameter constraints

The v1 parameter set adds `max_tx_units:u64` after `max_block_units:u64`.
`max_tx_bytes` is 256–262144, `max_block_bytes` is at most 1048576 and at
least `max_tx_bytes+16` so a maximal transaction fits an otherwise empty
body. `max_block_units` is 5000000–16777216 so the two mandatory per-height
records fit even at 9619 validators. `max_tx_units` is at least
`max_tx_bytes+2048`, at most 4194304 and at most half `max_block_units`.
`fee_per_unit` is a positive u64 minimum and the genesis initial congestion
price; `fee_base` is a nonnegative u64 per-transaction charge. A parameter
set is invalid if `fee_base + fee_per_unit*max_tx_units` exceeds u64. These
checks apply at genesis and to every scheduled governance change before it
can become active. Fee and limit changes take effect only at a finalized
epoch boundary under the committed parameter root.

The compact header adds `base_fee_per_unit:u64` immediately after
`parameter_root`. At height 1 it equals the genesis `fee_per_unit`. At height
`h > 1`, let `P` be the parent header's base fee, `U` its checked
`resource_units`, `M` its historical `max_block_units`, `T=floor(M/2)`, and
`F` the child parameter set's `fee_per_unit` floor. The next price is:

```text
if U > T: candidate = min(u64_max, P + max(1, floor(P*(U-T)/(8*T))))
if U = T: candidate = P
if U < T: candidate = P - floor(P*(T-U)/(8*T))
base_fee_per_unit = max(F, candidate)
```

The parent cap and usage determine the adjustment even across an epoch
boundary; the child floor then applies. A positive block is checked against
its own header price. Empty blocks lower the price toward the floor. The
`max(1, ...)` upward step prevents a price of one from becoming stuck under
congestion; u64 saturation cannot wrap. The parent and child parameter roots
identify the exact historical inputs. A mismatch in the header invalidates
the block.

For transaction units `u`, the minimum fee is
`fee_base + base_fee_per_unit*u`, calculated without overflow. The offered
u64 fee must meet or exceed it. The **entire offered fee is burned**; there
is no validator, proposer, treasury or refund split in v1. Overpayment is
also burned. This avoids introducing an unspecified reward or subsidy path.
No fee is charged for an invalid transaction because it cannot appear in a
valid block. If the minimum exceeds u64, that transaction is inadmissible
at that height; empty blocks still permit the base fee to decay.

Mempool admission simulates units against the current committed state and
next expected base fee, then rechecks at proposal time. Local ordering uses
offered fee per unit with exact cross multiplication, then arrival time and
transaction ID; it does not define block validity. Local per-sender pending
counts and resource quotas prevent one sender from monopolizing the pool.
A transaction may become temporarily inadmissible when state, limits or base
fee change; nodes evict or re-evaluate it without inventing a consensus
grandfather rule.

## Rationale and implementation boundary

Charging bytes, cryptographic verification, lot work, recorded state effects
and type-specific operations closes the flat-fee amplification in the
development protocol. Hard byte, count, per-transaction and block unit caps
bound work even when an attacker can pay. The parent usage and committed
child fee floor determine the next congestion price, so no local mempool,
wall clock or proposer vote selects a different price. Fixed coefficients
need independent measurement on the target execution engine before a public
v1 genesis; prices and hard caps must be reviewed together with worst-case
mandatory epoch work. The fixed surcharges do not excuse a state scan with
unbounded cost: every type and system transition must use bounded, indexed
access or a deterministic resumable cursor. Adversarial-state benchmarks
must show that the maximum admitted work fits the target block time and
memory budget; otherwise the limits or versioned cost schedule must change
before genesis.

The reference performs arithmetic and limit checks only. It cannot validate
typed bytes, infer state effects, verify signatures or replace the current
runtime's `FeeEconomics`. Phase 2.1 supplies typed bytes. Phase 3.10 and 3.12
must meter every execution, enforce limits during production and validation,
and replace absolute-fee mempool ordering. Phase 6 must replace fee splitting
and reconcile burn/supply accounting. An incompatible v1 genesis and replay
boundary are required; the `nodo/0.7` network must never claim these rules.
