# ADR 0011: Fixed-function v1 without smart-contract execution

**Status:** accepted Phase 1 design decision. It constrains the future v1
contract; the `nodo/0.7` development runtime is not v1. Phase 2.1 and the
execution gates in Phase 3 must implement the whole v1 contract before launch.

## Decision

Nodo v1 is a fixed-function ledger. Mainnet v1 does not require a virtual
machine, smart-contract deployment, arbitrary bytecode, scripts, programmable
accounts or user-defined state transitions. The only user transactions are the
13 numbered types in protocol v1 section 4. The only governance actions are
the five numbered kinds in section 3. An unknown type, action, enum, field,
trailing byte or noncanonical payload is invalid. There is no reserved
"contract call" type, opaque extension, fallback handler or plugin registry.
Relayers, wallets and off-chain programs may construct and submit valid typed
transactions but gain no new on-chain authority by doing so.

This choice keeps the launch state machine finite and reviewable. A VM would
add a separate consensus surface for instruction semantics, gas accounting,
storage growth and contract authority before the existing block, fee and
state rules are implemented. The v1 implementation therefore concentrates
review on staking, finality, governance and supply accounting.

The transaction `payload:bytes` is a length-delimited container for one exact
type-specific binary schema, not calldata. The `GOVERNANCE_PROPOSE`
`action:bytes` is likewise one exact action schema. Validators must decode the
entire payload, verify the type-specific amount and field rules, and re-encode
the same bytes before signature acceptance. No content in `title_hash`, text
proposals, network envelopes, peer records, transaction memo-like fields or
upgrade manifests can be interpreted as executable instructions under v1.
Governance `TEXT` has no state effect beyond its ordinary proposal lifecycle.
Governance may change only the allowlisted v1 parameters, spend treasury
lots under its fixed rule, or schedule/cancel a future protocol version as
specified by [ADR 0010](adr-0010-protocol-upgrades.md). It cannot deploy or invoke code
under v1 or grant a new transaction type by parameter vote.

V1 state has only the twelve tagged domains in protocol v1 section 3. There
is no contract account, storage namespace, code hash, code cache, call stack,
host function, event log or user-program-controlled privileged API. Every
state mutation is a deterministic effect of a named v1 transaction, accepted
evidence or required system transition. The proposer supplies data, never a
new execution procedure. Validators replay every effect and its receipt;
unknown effects invalidate the block. Coin creation, burn, staking, slashing,
treasury use and governance cannot be delegated to an unmetered script or
off-chain operator.

The bounded v1 resource meter in [ADR 0009](adr-0009-resource-fees.md)
charges exact transaction/receipt bytes, signatures, coin-lot operations,
recorded effects, evidence and system transitions. Its 13 type surcharges
cover only those fixed handlers. There is no VM gas schedule, execution
refund, dynamic call cost or contract storage rent in v1. Adding one without
a version change would make independent validation and fee agreement
impossible. Every handler must have a bounded input, bounded state access and
deterministic output under the existing block, transaction and system-work
limits. The implementation must use the same dispatch and rejection rules
in mempool admission, block production, voting, replay, sync and import.

The checked `V1FixedFunctionPolicy` is a Phase 1 reference for the closed
payload/action shapes and amount classes. It does not replace signature,
state, governance, fee or typed-codec validation. Phase 2.1 must make its
rules part of the canonical transaction decoder, and Phase 3 must remove
every permissive fallback in live execution paths before v1 activation.

## Future programmable version

Programmability is a separate, optional protocol decision after v1 launch,
not an implicit launch promise. If desired, it requires a new numbered
protocol version approved and activated through ADR 0010. Its rule bundle
must specify the VM/bytecode format or alternative execution model, exact
deterministic instruction and host-function semantics, isolation from
validator/treasury privileges, authorization and reentrancy rules, resource
metering and worst-case bounds, code and contract-storage state domains,
state-growth pricing, receipts/events, failure atomicity, migrations,
historical replay, snapshot proofs and cross-implementation vectors. It must
include adversarial fuzzing and independent security review before activation.
No v1 client may guess or execute those future rules.
