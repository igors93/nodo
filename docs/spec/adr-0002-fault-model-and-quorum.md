# ADR 0002: Byzantine fault model and strict weighted quorum

**Status:** accepted for the v1 design contract. The `nodo/0.3` development
runtime enforces the threshold arithmetic and parameter validity described
here; the complete locking, recovery, validator-set and liveness rules remain
roadmap work.

## Fault model

At every height `h`, let `V_h` be the authenticated, immutable validator-set
snapshot for that height. Let `W_h > 0` be the checked sum of its voting
weights and `B_h` the weight controlled by Byzantine validators. The safety
assumption is **`3 * B_h < W_h`**. It is about voting weight, not key count;
the [linear stake decision](adr-0001-validator-weight.md) prevents key
splitting from increasing a party's weight. Honest validators obey the
protocol, validate the full proposed block, and persist their last signed
vote, round and lock before sending any vote. Signatures cannot be forged and
the cryptographic commitments cannot be broken.

The adversary may control Byzantine validators, equivocate, withhold votes or
data, and delay, reorder, duplicate or partition messages. Safety must hold
without a bound on network delay. Liveness is conditional on **partial
synchrony**: after an unknown global stabilization time (GST), messages
between honest validators arrive within some finite unknown bound; timeouts
eventually exceed that bound; an honest proposer eventually gets a round; and
the proposal data is available. The protocol does not promise progress during
a partition or when `3 * B_h >= W_h`. Quorum arithmetic alone does not prove
that Nodo's current locking and timeout implementation meets these promises.

## Quorum rule

For each height, the sole accepted threshold is
`Q_h = floor(2 * W_h / 3) + 1`. A vote or quorum certificate with exactly
two thirds of the total weight is insufficient. For positive `u64` total
weight, an overflow-free implementation is `W_h - (W_h - 1) / 3`. Zero total
weight is invalid. The only valid serialized fraction is numerator 2,
denominator 3. Alternate encodings such as 4/6, lower thresholds, and higher
thresholds all require a new protocol design and version. A higher threshold
would make the stated liveness fault bound too optimistic.

An accepted PRECOMMIT certificate must carry exactly `Q_h` as its required
weight, identify the validator-set snapshot for `h`, contain unique valid
signatures from that set for one block and round, and have signed weight at
least `Q_h`. A certificate is checked against the historical set, never the
current live registry. PREVOTE certificates may justify locks or round
changes under the consensus state machine; they never finalize a block.

Two subsets each weighing at least `Q_h` intersect in weight at least
`2 * Q_h - W_h > W_h / 3`. Under the fault bound, that intersection contains
honest weight. Also, `W_h - B_h >= Q_h`, so honest weight alone can form a
certificate once communication and proposal conditions hold. These are
arithmetic properties, not a substitute for the lock and view-change proof:
honest validators must never sign conflicting certificates across rounds.

## Consequences

The development implementation rejects noncanonical network fractions at
configuration validation, uses the same strict rule for vote pools and QC
creation, rejects forged required-weight metadata during structural and
cryptographic QC validation, and rejects old `nodo/0.2` profiles under
`nodo/0.3`. Regression tests cover divisible totals, adjacent thresholds,
large `u64` totals, profile rejection, and the quorum-intersection and
honest-progress inequalities.

This decision does not close the separate work on durable vote persistence,
correct locks and round changes, epoch-only validator-set transitions,
historical set proofs, data availability, or a formal model. Those are
required before production. The [CometBFT consensus specification](https://github.com/cometbft/cometbft/blob/main/spec/consensus/consensus.md)
and [core data structures](https://github.com/cometbft/cometbft/blob/main/spec/core/data_structures.md)
are non-normative references for the strict weighted threshold.
