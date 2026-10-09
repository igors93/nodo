# Nodo protocol specification v1 (design contract)

**Status:** design contract for a future, incompatible rule set. No existing
network or `nodo/0.x` artifact is a v1 artifact. This document does not enable
production operation. The roadmap tracks implementation and external review
separately. In this document **MUST**, **MUST NOT**, and **SHOULD** are normative.
This document, including the ADR rules it explicitly incorporates, defines
v1 consensus; the other protocol pages describe the current implementation or
give background. An implementation MUST reject an
object when it cannot prove every applicable rule below. A local node MUST NOT
advertise v1 or sign a v1 vote until it implements the whole contract.

## 1. Network identity, activation and limits

A v1 chain starts from one immutable, canonically encoded genesis document. Its
`chain_id` is 1–64 lowercase ASCII letters, digits, `-` or `_`; `genesis_hash`
is the hash of that document. Both identify the network in every signed object
and handshake. Genesis commits to the initial state, initial active validator
set, protocol version `1`, its nonzero content-addressed `rule_set_hash`,
cryptographic suite `Ed25519-SHA256`, initial
consensus parameters, and the complete parameter schedule. There is no default
value for a missing genesis field. Nodes MUST pin the expected genesis hash out
of band. A different genesis hash is a different chain, even with the same name.

The genesis parameter record contains positive `epoch_length_blocks` (`L`)
and `target_block_seconds` (`T`), with `T <= 300` and the checked product
`86400 <= L*T <= 604800` (one to seven nominal days), plus `min_validator_stake`,
`max_tx_bytes` (at most 262144),
`max_block_bytes` (at most 1048576),
`max_block_units`, `max_tx_units`, `fee_base`, `fee_per_unit`, `min_proposal_deposit`,
`max_treasury_spend_per_epoch`, and `treasury_timelock_epochs`. Genesis MUST
contain at least four active validators with positive weight. It also contains
`max_epoch_churn_basis_points` (1–3333 of previous total weight),
`activation_delay_epochs` (at least 2), `unbonding_seconds` (at least 28 days),
`evidence_max_age_seconds` (at most 21 days and strictly less than unbonding),
`weak_subjectivity_seconds` (at most 14 days), and
`max_future_skew_seconds` (at most 30). Resource and fee parameters obey
[ADR 0009](adr-0009-resource-fees.md), including a positive `fee_per_unit`
floor and a checked, finite per-transaction fee. All amounts and costs are unsigned
raw units (1 NODO = 100000000 raw units); checked arithmetic MUST reject
overflow. The only encoded quorum fraction is numerator 2, denominator 3;
the required weight is **strictly more than two thirds**:
`floor(2 * W / 3) + 1`. Governance cannot change this rule.
Genesis MUST allocate and account for every initial coin lot and treasury coin.
Genesis MUST define an issuance schedule as sorted, nonoverlapping, finite
`(first_epoch:u64, last_epoch:u64, units_per_epoch:u64)` ranges; epochs not
covered by a range issue zero; epoch 0 MUST issue zero outside genesis
allocation. This schedule is immutable in v1. V1 does not
infer an issuance rate from wall time or a software default.

Genesis is height 0. Consensus epoch `e` (zero based) contains heights
`[e*epoch_length_blocks+1, (e+1)*epoch_length_blocks]`. The parameter set and
validator set for a height are the versions committed by the **previous
finalized** epoch boundary. Epoch 0 uses the genesis set and parameters. For
`e > 0`, the last block of epoch `e-1` commits the versions used from height
`e*epoch_length_blocks+1`. A governance parameter change may affect only fee
rates, resource limits within the hard caps above, or the treasury spend cap;
it takes effect at a named epoch at least two epochs after execution. Changes
to encoding, signatures, consensus, time, issuance, validator eligibility,
slashing, or governance require a new protocol version and an explicit
activation height. [ADR 0010](adr-0010-protocol-upgrades.md) fixes the
approved schedule, four-full-epoch notice, boundary commitment and
fail-closed activation. Nodes MUST retain historical rule sets to replay old
blocks. Unknown versions and unscheduled transitions are rejected. No v1
mainnet genesis or activation height is defined by this repository.

Epoch membership depends only on height: at positive height `h`,
`e = floor((h-1)/L)` and a finalized block is an epoch boundary exactly when
`h mod L = 0`. Genesis has no epoch. Every boundary and activation height uses
checked `u64` arithmetic. The boundary block itself uses the old set and
parameters; the next height uses its committed successors. The exact BFT-time
rule below guarantees `header.time >= genesis.time + h*T` with checked `i64`
arithmetic. `L*T` is the nominal minimum from one epoch boundary to the next
(or genesis to the first boundary), never a wall-clock trigger for epoch
transitions. A stalled or delayed chain does not skip epochs or issue coins
for elapsed calendar time. Epoch `e` issues its scheduled exact units once in
its final block `(e+1)*L`; epoch 0 issues zero outside genesis. The schedule
is not an implicit annual rate; evidence and unbonding
seconds use BFT header time, while epoch delays use height-derived indices.
There is no independent `epoch_duration_seconds` or `epochs_per_year` v1
parameter. [ADR 0007](adr-0007-epoch-cadence.md) fixes the cadence decision.

## 2. Canonical bytes and cryptography

All consensus objects are binary. A top-level object is `magic="NODO"` (four
ASCII bytes), `version:u16=1`, `kind:u16`, followed by the kind's fixed schema.
The prefix is exactly eight bytes. A nested object is `length:u32` followed
by that object's schema bytes without another top-level prefix; fixed-size
hashes, keys and signatures are the only nested values without a length.
Integers are unsigned big-endian unless explicitly `i64` or `i128`
(fixed-width two's complement big-endian). `bool` is one byte, exactly `00`
or `01`. `bytes` is a `u32`
length followed by that many bytes. UTF-8 fields MUST be valid UTF-8 in NFC,
with no NUL; fields used as identifiers are restricted to their stated ASCII
alphabet. Fixed-size hashes, keys and signatures carry no length prefix. Lists
are `u32 count` followed by elements; maps are never encoded. Optional values
are a one-byte presence flag followed by the value if present. There are no
unknown fields, duplicate keys, implicit defaults, padding, trailing bytes,
floats, locale-dependent text, or extension fields. Every decoder MUST enforce
the object's byte and count limits *before* allocation and MUST require full
input consumption. Re-encoding a decoded object MUST reproduce exactly the
original bytes.

`H(domain, x) = SHA-256(bytes("NODO/V1/") || ASCII(domain) || 0x00 || x)`.
Every domain below is distinct and fixed. Transaction ID is `H("TXID",
signed_transaction_bytes)`. Block ID is `H("BLOCK", header_bytes)`. A signature
is Ed25519 over `H(sign_domain, unsigned_object_bytes)`. For transactions,
votes, proposals and envelopes, those bytes are the complete top-level prefix
and fields before the signature; they include version, chain ID and genesis
hash. Handshake authentication uses the separately prefixed transcript in
[ADR 0008](adr-0008-canonical-binary.md). The
closed signing-domain registry is `SIGN/TX` for transactions, `SIGN/VOTE` for
votes, `SIGN/PROPOSAL` for consensus proposals, `SIGN/ENVELOPE` for network
envelopes, `SIGN/HANDSHAKE` for handshake transcripts and `SIGN/PEER` for
signed peer records. Public
keys are exactly 32 bytes, signatures exactly 64 bytes, hashes exactly 32
bytes. Account ID is `H("ACCOUNT", public_key)`, peer ID is
`H("PEER", peer_public_key)`, and validator ID is
`H("VALIDATOR", consensus_public_key)`. There is no BLS or text-signing
fallback in v1. Reused keys across chains cannot replay signed objects because
chain ID and genesis hash are signed. The same domain rule applies to votes,
proposals, handshake transcripts and evidence.

`genesis_hash = H("GENESIS", genesis_bytes)`. `validator_set_root =
H("VALSET", validator_set_bytes)`, `parameter_root = H("PARAMS",
parameter_set_bytes)`, and `parent_qc_hash = H("QC", parent_qc_bytes)`.
For every named top-level object hash, these inputs are the **complete top-level bytes**
including the eight-byte prefix, even if the object was transported nested.
Evidence ID is `H("EVIDENCE", evidence_bytes)` and a system-record ID is
`H("RECORD", record_bytes)`.
`tx_root`, `receipt_root` and `evidence_root` are the Merkle roots of their
complete top-level element bytes with kinds `tx`, `receipt` and `evidence`
respectively. State leaves and system records use their exact nested schema
bytes because neither has a top-level kind. `body_root =
H("BODY", complete_top_level_body_bytes)` commits to the entire body,
including deterministic system-transition records; the header's `body_bytes`
field is the length of those complete top-level bytes.
`state_root` uses kind `state`. A signature never substitutes for a root or a
root for a signature. The binary digest, rather than its hexadecimal display,
is embedded in other objects.

Merkle roots use `H("MERKLE-LEAF/" + kind, u32(index)||element_bytes)` and
`H("MERKLE-NODE/" + kind, left||right)`. For `n>1`, split at the largest
power of two strictly less than `n`, recursively hash both sides, and never
duplicate a leaf. The nonempty root is
`H("MERKLE-ROOT/" + kind, u32(n)||tree_hash)`; the empty root is
`H("MERKLE-EMPTY/" + kind, empty)`. Allowed kinds are `tx`, `receipt`,
`evidence` and `state`. List order is
significant unless a schema explicitly requires sorting. State leaves are
`domain:u8 || key:bytes || value:bytes`, sorted lexicographically by the full
encoded key; duplicate keys are invalid. The state root is the Merkle root of
these leaves. Absence proofs MUST use the same ordered tree definition. Hashes
of transactions, receipts, evidence, validator sets and parameters use their
respective canonical bytes and domain strings; no text representation may be
hashed or signed.

The top-level kind registry is: `1 genesis`, `2 transaction`, `3 header`,
`4 block body`, `5 receipt`, `6 vote`, `7 proposal`, `8 quorum certificate`,
`9 evidence`, `10 validator set`, `11 parameter set`, `12 finalized artifact`,
`13 state snapshot`, `14 network envelope`. A change to a field, tag or hash
rule requires a new version. Object schemas in sections 3–7 list fields in
wire order; `hash` means 32 bytes, `account`/`validator` means their 32-byte
IDs, `key` means 32 bytes, `sig` means 64 bytes, and `str` means UTF-8 `bytes`.
The exact top-level byte caps are: transaction and evidence 262144; header
4096; vote 1024; receipt 65536; genesis, body, QC, validator set and parameter
set 1048576; proposal, finalized artifact, snapshot and envelope 4194304.
The frame cap is 5242880. Counts may not exceed 1048576 even when the byte cap
would allow more, and lower field-specific caps apply. This registry and the
full nested wire schema are fixed by [ADR 0008](adr-0008-canonical-binary.md).

The remaining container schemas are fixed as follows. Genesis is `chain_id,
genesis_time:i64, rule_set_hash:hash, parameter_set, validator_set, initial_accounts:list<key>,
initial_lots:list<lot>, initial_stakes:list<stake>,
issuance_ranges:list<range>`; account keys sort by derived account ID, lot
entries by lot ID, stakes by position ID and ranges by first epoch. A genesis lot is
`lot_id:hash, origin_id:hash, owner:account, amount:u64, status:u8` with
status AVAILABLE, STAKED or TREASURY. A genesis stake is `position_id:hash,
owner:account, validator_id:hash, lot_ids:list<hash>` with sorted unique lot
IDs. Every non-treasury genesis lot owner MUST derive from an initial account
key. Every STAKED genesis lot MUST belong to exactly one genesis stake, and
each initial validator's weight MUST equal the sum of its staked lots. A
parameter set encodes the 21 typed fields in the exact order of
[ADR 0008](adr-0008-canonical-binary.md); a validator-set entry
is `validator_id, owner:account, consensus_key, weight:u64, status:u8`, sorted
by validator ID. A consensus proposal (kind 7, distinct from a governance
proposal transaction) is `header, body, valid_round:optional<u64>,
valid_prevote_qc:optional<qc>, signature:sig`. Both optional fields MUST be
present together, and the QC MUST certify the proposed block in `valid_round`.
A body is `transactions:list<transaction>,
evidence:list<evidence>, system_records:list<record>`; receipts are
`tx_or_record_id:hash, units:u64, fee:u64, effect_ids:list<hash>,
post_state_root:hash`. A finalized artifact is `header, body,
receipts:list<receipt>, parent_qc:optional<qc>, final_qc:qc`.
Evidence is `kind:u8, first_signed_object:bytes,
second_signed_object:bytes`; only conflicting votes for one
`(chain,height,round,step,validator)` and conflicting proposals for one
`(chain,height,round,proposer)` are accepted in v1. The two objects sort by
their canonical byte strings. A snapshot is `height:u64, header_id:hash,
protocol_version:u16, rule_set_hash:hash, state_root:hash,
state_leaves:list<state_leaf>, finality_path:list<qc>`;
leaves sort by state key. Every nested structured object has a `u32` byte
length and its canonical schema without a second magic/version prefix.
System records are tagged `slash`, `stake_maturity`, `validator_set_change`,
`governance_decision`, `treasury_execution`, `epoch_issuance`,
`parameter_change`, `upgrade_schedule`, `proposer_schedule`, or
`liveness_window`, followed by the affected ID,
previous value hash and new
value bytes. Their order is the system-transition order in section 3, then
affected ID ascending within a tag. A record with no actual state change is
invalid. This rule also fixes receipt and root order.

## 3. State machine

The canonical state is a tuple of account records, coin-lot records, staking
positions, validator registry and pending set changes, governance proposals and
votes, treasury balance and execution IDs, supply counters, protocol parameter
schedule, protocol upgrade schedule, proposer-priority schedule,
QC-participation window, and processed
evidence IDs. Every state
key belongs to exactly one domain. At an upgrade height, the versioned
deterministic migration and schedule activation execute once on the old
finalized state before any user transaction. The new version defines their
exact state effects, receipts, system records and resource accounting;
failure invalidates the block. A transaction executes atomically in block
order. Any invalid
transaction invalidates its containing block; there is no partial success or
fee deduction on failure. Receipts for valid transactions record transaction
ID, resource units, charged fee, ordered state-effect IDs and post-transaction
state root. System transitions run **after** user transactions in this order:
accepted evidence and slashing, matured stake/validator changes, governance
decision and execution records (including upgrade schedule/cancel),
parent-QC liveness assessment, epoch settlement, next-epoch set/parameter and
next-rule commitments, liveness-window update/reset, and the
proposer-priority update/rebase. Each committed transition
emits a typed receipt and ledger record.
At each height, the required issuance and already committed parameter/set
transition costs are reserved before user transactions. Evidence inclusion
requires room for its own verification and slash transition. At a boundary,
if matured optional queue entries exceed the remaining byte, record-count or
unit budget, process the deterministic prefix in transition order and affected
ID order; retain the remainder for the next eligible boundary. An action with
a fixed activation epoch is invalid when scheduled if its required work cannot
fit the reserved budget at that epoch. A proposer cannot omit required work to
make room for a fee-paying transaction.

State-domain tags and value schemas are fixed below. A state key is the tag
followed by its fixed-width key, except the composite governance-vote key.
The values use the primitive encoding of section 2. A missing account has
nonce and balance zero; no other missing key has an implicit default. The
treasury account ID is `H("TREASURY", empty)`. An unknown domain tag or
user-defined storage key invalidates the state root. V1 has no contract
account, code store or user-program-controlled state namespace; see
[ADR 0011](adr-0011-fixed-function-v1.md).

| Tag | Key | Value fields in wire order |
| --- | --- | --- |
| 1 account | account ID | `nonce:u64, available:u64, key:key` |
| 2 coin lot | lot ID | `origin_id:hash, owner:account, amount:u64, created_height:u64, status:u8, unlock_time:i64` |
| 3 stake | position ID | `owner:account, validator:validator, locked_lots:list<hash>, unbonding_lots:list<hash>, eligible_epoch:u64` |
| 4 validator | validator ID | `owner:account, key:key, status:u8, activation_epoch:u64, exit_epoch:u64, jailed_until:i64` |
| 5 proposal | proposal ID | `kind:u8, action:bytes, deposit_lot:hash, open_epoch:u64, close_epoch:u64, expiry_epoch:u64, decision:u8` |
| 6 governance vote | proposal ID `||` validator ID | `choice:u8, snapshot_weight:u64` |
| 7 treasury execution | proposal ID | `recipient:account, amount:u64, execution_epoch:u64` |
| 8 supply | 32 zero bytes | `genesis:u64, minted:u64, burned:u64, slashed_burned:u64` |
| 9 parameter schedule | epoch as `u64` | canonical parameter-set bytes |
| 10 evidence | evidence ID | `inclusion_height:u64, offender:validator, slash:u64` |
| 11 upgrade schedule | activation height as `u64` | `from_version:u16, to_version:u16, previous_rules_hash:hash, next_rules_hash:hash, rule_bundle_hash:hash, vectors_hash:hash, migration_hash:hash, status:u8` |
| 12 proposer schedule | 32 zero bytes | `set_root:hash, last_finalized_height:u64, total_weight:u64, entries:list<proposer_entry>`; each entry is a length-prefixed 48-byte `validator_id:validator, priority:i128` |
| 13 liveness window | 32 zero bytes | `set_root:hash, last_finalized_height:u64, epoch:u64, observed_heights:u64, total_weight:u64, entries:list<liveness_entry>`; each entry is a length-prefixed 48-byte `validator_id:validator, weight:u64, signed_heights:u64` |

The proposer-schedule leaf exists from genesis. Its entries match the frozen
active validator set for the next height exactly and its set root and total
weight match that set. Genesis priorities and `last_finalized_height` are
zero; after height `h`, the latter is exactly `h`. After every finalized
height, the mandatory proposer-priority transition and any boundary rebase
follow [ADR 0012](adr-0012-proposer-selection.md); the next state root
commits the result. System-record tag 9 commits the derived next value by
hash. Its zero-fee receipt has the sole effect ID
`H("PROPOSER-SCHEDULE", empty)`; the transition consumes the reserved
system budget. Missing, extra or malformed priority
entries invalidate a block or imported snapshot.

The liveness-window leaf exists from genesis with zero counters. At height
`h>1`, the verified parent PRECOMMIT QC supplies the only signer list that
may increase counts for height `h-1`. The boundary QC is excluded because it
is unknown when that boundary header commits the next set. The boundary
assesses exactly `L-1` preceding QCs using
[ADR 0013](adr-0013-liveness-accountability.md), then resets the window
onto the next set. System-record tag 10 commits the derived next value by
hash and has one zero-fee receipt. Missing, extra or fabricated counts,
manual penalties, or a slash based on QC absence invalidate the candidate.

Upgrade status is 1 PENDING, 2 ACTIVE or 3 CANCELED. An entry is created
only by an approved upgrade action, becomes ACTIVE exactly at its activation
height, and can become CANCELED only by an approved cancellation before that
height. Its key and content hashes never change. The genesis rule-set hash
anchors version 1; it has no schedule entry. The `upgrade_schedule` system
record (tag 8) commits each schedule insertion, cancellation or activation
as a state change, in governance-execution order for insertion/cancellation
and before user transactions for activation. The fixed activation transition
must fit the reserved system budget. Its `affected_id` is
`H("UPGRADE-ID", u64(activation_height))`; its `new_value` is the exact tag-11
value bytes and `previous_value_hash` is
`H("STATE-VALUE", previous_exact_value_bytes)`,
or zero for insertion.

All lot-ID lists are strictly sorted. Lot status tags are 1 AVAILABLE,
2 STAKED, 3 UNBONDING, 4 TREASURY, 5 SPENT, 6 BURNED. Validator status tags
are 1 PENDING, 2 ACTIVE, 3 EXITING, 4 JAILED, 5 EXITED. Governance decision
tags are 1 OPEN, 2 APPROVED, 3 REJECTED, 4 EXECUTED, 5 EXPIRED. These tags
are consensus bytes, not display strings. Spent/burned lot tombstones remain
in state to prevent ID reuse; only AVAILABLE, STAKED, UNBONDING and TREASURY
lots count toward live supply.

Accounts hold `nonce:u64` (initially zero), available balance, and key ID.
Every balance is derived from unspent available coin lots owned by that account.
Each lot has immutable `lot_id`, `origin_id`, amount and creation height, plus
current owner/status (`AVAILABLE`, `STAKED`, `UNBONDING`, `TREASURY`, `SPENT`,
`BURNED`). Spending consumes whole input lots and creates deterministic output
and change lots with IDs `H("LOT", tx_id||u32(output_index))`; zero outputs are
forbidden. Inputs are explicitly named, strictly sorted by lot ID, unique,
owned by the sender and `AVAILABLE`. Input sum equals value outputs plus fee
plus change. A stake operation reclassifies lots without minting them. Supply
obeys `genesis + scheduled_mint - burns - slashes_burned = sum(live_lots)`.
Fees are burned in v1; overpayment is also burned. Treasury transfers conserve
supply. No operation can create unbacked balance, negative amount or supply
outside the genesis issuance schedule.

Staking positions bind an owner to a validator and a set of stake lots.
Consensus weight is **linear raw active locked stake**; splitting stake does
not increase aggregate weight. Each unit of eligible voting weight MUST be
backed by exactly one active, locked, unspent stake unit, including at genesis.
The same lot MUST NOT back two validators. No operator-supplied weight, square
root, per-validator weight cap, or top-K selection is allowed in v1. Every
eligible candidate meeting `min_validator_stake` joins the active set after
the activation delay and churn rules; an identity limit chosen by descending
stake would let a split stake displace honest validators. The minimum stake
limits cheap identities but is not the Sybil safety argument. A future bound on
active-set size requires a separately analyzed, versioned selection mechanism
that preserves the adversary's stake fraction under arbitrary splitting.
At each epoch boundary, queued reductions and removals are processed first;
remaining changes are ordered by validator ID. Churn is
the sum of absolute weight changes over the union of old and new validator
IDs, bounded by `floor(previous_total_weight *
max_epoch_churn_basis_points / 10000)`. A candidate needs at least
`min_validator_stake` and a distinct valid consensus key. A queued weight
change larger than the remaining churn allowance is applied partially in
whole raw units; the remainder stays queued. An entry below minimum stake is
deferred. Evidence-driven jail removes the validator at the next boundary even
if this exceeds the voluntary churn cap; syncing nodes must verify each
intermediate set and QC across that boundary.
Set changes activate
only after `activation_delay_epochs` (at least two finalized boundaries), and
the last block of epoch `e` commits
to the set for epoch `e+1`. Exit, jailing, key rotation and stake changes never
rewrite the set used at an earlier height. If the churn bound would be exceeded, excess
changes wait in deterministic candidate order. A validator cannot withdraw
until BFT time has advanced by `unbonding_seconds` after unlock *and* all
evidence within the allowed window has been resolved. Evidence age is measured
from the authenticated BFT header time of the finalized parent at the offense
height (genesis for height 1), never from an accused validator's claimed vote
time or a reporting peer's detection time. At inclusion, the checked difference
`inclusion_header_time - offense_parent_header_time` MUST be between zero and
`evidence_max_age_seconds`, inclusive; an overflow or missing canonical
offense parent invalidates the evidence. Accepted evidence IDs are
idempotent. `double_vote_slash_bps` and `double_proposal_slash_bps` are
immutable genesis parameters in 1–10000. For each accepted evidence record,
burn `min(current_locked_stake, ceil(stake_at_offense * fraction / 10000))`
from the offender's lots in ascending lot-ID order and jail until at least
`evidence_inclusion_BFT_time + jail_seconds`; `jail_seconds` MUST be at least
`unbonding_seconds`. A partial lot burn creates a new remainder lot with a
deterministic evidence-derived ID. The same evidence cannot slash twice.
Jailed stake stays slashable through unbonding.

Inactivity is separate from cryptographic equivocation evidence. A validator
with fewer than 75% of its `L-1` selected finalized-QC opportunities may be
reversibly jailed at the next epoch boundary under the ordinary churn cap,
with at least four validators remaining and a checked one-day BFT-time jail
deadline. This action never burns stake or creates an evidence ID. A QC can
omit a valid vote, so QC absence is not slashable proof of operator fault.
There is no manual consensus penalty. ADR 0013 fixes exact accounting,
assessment order, deferral and re-entry.

Governance has `PARAMETER_CHANGE` (kind 1), `TREASURY_SPEND` (2), `TEXT` (3),
`PROTOCOL_UPGRADE` (4) and `UPGRADE_CANCEL` (5) proposals. Kinds 4 and 5
have the exact action bytes and scheduling constraints of
[ADR 0010](adr-0010-protocol-upgrades.md); unknown kinds are invalid.
The complete action bytes are respectively
`parameter_tag:u8, new_value:u64, activation_epoch:u64`,
`recipient:account, amount:u64`, empty,
`next_version:u16, activation_height:u64, rule_bundle_hash:hash,
vectors_hash:hash, migration_hash:hash`, or
`activation_height:u64, rule_set_hash:hash`. Parameter tags are 1 `fee_base`,
2 `fee_per_unit`, 3 `max_tx_bytes`, 4 `max_block_bytes`, 5 `max_tx_units`,
6 `max_block_units`, and 7 `max_treasury_spend_per_epoch`; no other parameter
is governable in v1. U32 parameters encoded through `new_value:u64` must
fit u32 as well as their protocol hard caps. The only vote choice tags are
1 YES, 2 NO and 3 ABSTAIN.

Proposal ID is its transaction ID. A proposal opens at the next epoch,
accepts votes for two full epochs, and snapshots the validator set and linear
weights at opening. Each owner may cast one signed vote per proposal for its
validator (`YES`, `NO`, `ABSTAIN`); replacement votes are invalid. Quorum is
strictly more than two thirds of snapshot weight participating. Approval is
strictly more than two thirds of `YES+NO` weight, with nonzero `YES` weight.
Uncast weight does not count as approval. The decision is deterministic at the
first block after voting closes. An approved action can be executed only once,
after `treasury_timelock_epochs`, before its explicit expiry epoch. A text
proposal has no executable state effect. Parameter actions are limited to the
allowlist in section 1 and cannot retroactively affect validation. Upgrade
actions cannot execute unless their notice, version, content commitments and
sole-pending-entry invariants hold; cancellation requires the same governance
authorization and must finalize before its boundary commitment.

Treasury coins are lots owned by the reserved treasury account. A spend
requires a positive action amount, the exact approved proposal ID, matching
recipient and amount, unexpired decision, elapsed timelock, per-epoch cap
and sufficient treasury lots. Executing consumes treasury lots and creates
recipient/change lots.
Direct transfers from the treasury account and independent minting are invalid.
Epoch issuance, if nonzero in genesis, creates only the scheduled issuance lots
in the treasury at the epoch boundary; allocation to rewards requires a
separately versioned, deterministic reward rule. V1 does not award subjective
Proof of Protection scores or automatic staking rewards.

## 4. Transactions and deterministic rejection

Transaction fields in order are `chain_id:str, genesis_hash:hash,
type:u8, sender_key:key, nonce:u64, expiry_height:u64, amount:u64,
fee:u64, input_lots:list<hash>, payload:bytes, signature:sig`. The signed
bytes exclude only `signature`. `nonce` MUST equal account nonce plus one;
`expiry_height` MUST be at least the inclusion height and at most that height
plus two epochs. The sender is derived from `sender_key`. Transactions MUST
have at least one input lot whenever they debit coins. Empty payloads are
valid only for the types whose schema is empty. A successful transaction
increments nonce exactly once. Mempool replacement and ordering are local
policy, never validity rules.

V1 is a fixed-function chain: these 13 types are the entire executable
user-transaction registry. The `payload:bytes` contents must decode to the
one exact schema below with full consumption. There is no contract-call,
deployment, VM, script or extension type. Unknown types and extra bytes are
invalid, including when carried in a validly signed transaction.

| Type tag | Payload fields in wire order | Valid state effect |
| --- | --- | --- |
| 1 TRANSFER | `recipient:account` | Move positive `amount` to recipient; sender differs. |
| 2 BURN | empty | Burn positive `amount`. |
| 3 STAKE_DEPOSIT | `validator:validator` | Create a stake position from positive `amount`. |
| 4 STAKE_UNLOCK | `position:hash` | Move positive `amount` from locked to unbonding. |
| 5 STAKE_WITHDRAW | `position:hash` | Return mature positive `amount` to owner. |
| 6 STAKE_TOP_UP | `position:hash` | Add positive `amount` to owned position. |
| 7 VALIDATOR_REGISTER | `consensus_key:key, position:hash` | Register an unused key backed by qualifying stake. |
| 8 VALIDATOR_EXIT_REQUEST | `validator:validator` | Queue exit at a future epoch. |
| 9 VALIDATOR_UNJAIL_REQUEST | `validator:validator` | Queue unjail after jail expiry and remedied stake. |
| 10 VALIDATOR_KEY_ROTATE | `validator:validator, new_key:key, activation_epoch:u64` | Queue unique key at a future epoch. |
| 11 GOVERNANCE_PROPOSE | `kind:u8, title_hash:hash, action:bytes, expiry_epoch:u64` | Lock proposal deposit and create proposal. |
| 12 GOVERNANCE_VOTE | `proposal:hash, choice:u8, validator:validator` | Record one snapshot-weight vote. |
| 13 GOVERNANCE_EXECUTE | `proposal:hash` | Execute exact approved action once. |

For types 1–6 and 11, `amount` MUST be positive. For types 7–10 and 12–13,
`amount` MUST be zero. Type 11 `amount` is the proposal deposit and MUST meet
`min_proposal_deposit`. Its `action:bytes` must have the exact schema for its
kind in section 3; the nested `bytes` length is checked before reading and
trailing bytes are invalid. `TEXT` has an empty action, and its `title_hash`
is inert metadata. No arbitrary code or opaque state-changing payload is
valid. The proposal deposit is refunded on approval and burned on
rejection; its minimum is the genesis parameter. For stake unlock/withdraw,
`amount` identifies the portion of the position, and only the fee is drawn
from available input lots. The offered fee participates in the required
debit and any positive change creates a new coin lot; fee burns create none.
The required debit is `amount + fee` for types 1, 2, 3, 6 and 11; `fee` for
all others. The primary output is absent for type 2. The normative
[resource and fee schedule](adr-0009-resource-fees.md) derives `tx_units`
from complete transaction and receipt bytes, one signature, input and
new-lot counts, receipt effect IDs and the fixed type surcharge.
`input_lots <= 128`, newly created lots `<= 2`, and receipt effect IDs
`<= 256`. The complete transaction must fit `max_tx_bytes`, and its units
must fit `max_tx_units`. The transaction's offered fee must be at least
`fee_base + header.base_fee_per_unit * tx_units`, using checked arithmetic;
the entire offered fee is burned. Validators re-execute and verify the
receipt units and fee. Evidence and system transitions also consume block
units, though their receipts have zero fee. User transactions may consume at
most three quarters of `max_block_units`; all work together must fit that
limit. A proposer cannot make an invalid transaction valid by claiming a
different receipt or unit count.

The following is the **closed** rejection-code registry. Validators check in
table order, then type-specific predicates in the order shown. The first
failure is the diagnostic code; codes do not appear as failed receipts in a
valid block. Unknown conditions map to `INTERNAL_UNDEFINED` and halt signing,
not to a permissive fallback. Peer-specific codes are not consensus data.

| Order | Code | Exact failure class |
| --- | --- | --- |
| 1 | `ENCODING` | Bad magic/version/kind, malformed length, noncanonical scalar, UTF-8, list order, duplicate or trailing bytes. |
| 2 | `SIZE` | Transaction, field, list, block or resource cap exceeded before allocation. |
| 3 | `NETWORK` | Chain ID, genesis hash, rule version or historical activation mismatch. |
| 4 | `TYPE` | Unknown transaction type, tag, payload shape, enum or prohibited amount. |
| 5 | `AUTH` | Missing/invalid signature, key or sender/key binding. |
| 6 | `DUPLICATE` | Transaction ID, input lot, evidence ID, or block transaction repeated. |
| 7 | `HEIGHT` | Expiry, activation or inclusion height outside its valid window. |
| 8 | `NONCE` | Sender nonce not exactly next expected value. |
| 9 | `FEE` | Insufficient fee, arithmetic overflow, or invalid fee input. |
| 10 | `LOT` | Lot absent, unavailable, wrong owner, noncanonical inputs, incorrect conservation or output. |
| 11 | `BALANCE` | Available funds or protocol escrow insufficient. |
| 12 | `STAKE_OWNER` | Stake position absent or controlled by another owner. |
| 13 | `STAKE_STATE` | Invalid deposit/top-up/unlock/withdraw amount, maturity or evidence hold. |
| 14 | `VALIDATOR_KEY` | Duplicate/invalid consensus key, identity binding or rotation target. |
| 15 | `VALIDATOR_STATE` | Ineligible registration, exit/unjail/rotation state or epoch. |
| 16 | `PROPOSAL` | Invalid proposal kind, action, deposit, lifetime or restricted parameter. |
| 17 | `VOTE` | Closed/unknown proposal, non-snapshot voter, duplicate vote or invalid choice. |
| 18 | `DECISION` | Not approved, wrong action, timelock, expiry or already executed. |
| 19 | `TREASURY` | Recipient/amount mismatch, insufficient lots or epoch spend cap exceeded. |
| 20 | `SUPPLY` | Any supply, lot or issuance invariant violation. |
| 21 | `INTERNAL_UNDEFINED` | Implementation lacks a rule required to decide validity; fail closed. |

## 5. Blocks, consensus and finality

Header fields in order are `chain_id:str, genesis_hash:hash, height:u64,
protocol_version:u16, rule_set_hash:hash, next_protocol_version:u16,
next_rule_set_hash:hash, round:u64, parent_id:hash, time:i64,
proposer:validator, validator_set_root:hash, next_validator_set_root:hash,
parameter_root:hash, base_fee_per_unit:u64, parent_qc_hash:hash, body_root:hash, tx_root:hash, receipt_root:hash,
evidence_root:hash, state_root:hash, body_bytes:u32, resource_units:u64`.
The block body is ordered transactions then sorted unique evidence records and
the deterministic system-transition records. The block ID covers only the
compact header; roots commit to the entire body and resulting state. Genesis
uses height 0, zero parent/QC hashes, and its own rules. Every later header
MUST match the active and next rule commitments derived from the authenticated
schedule. At the last old-rule height before activation, its finality QC
authenticates the exact next version/hash; the next block uses that version's
prefix and rules. An unsupported or mismatched version MUST halt validation
and signing, never fall back to the prior version. Every later header
must have the prior finalized block ID and a verified parent PRECOMMIT QC.
Height 1 is the sole exception: its parent is genesis, `parent_qc_hash` is all
zero bytes and `parent_qc` is absent. It uses the genesis validator-set
commitment as its trust anchor. Body, receipt, transaction, evidence, state, set and parameter roots
MUST be recomputed, never trusted from a peer. The header's
`base_fee_per_unit` is checked from the parent header's price, metered units
and historical block limit, then the active child fee floor. Its
`resource_units` is the exact sum of transaction, evidence and system work
under [ADR 0009](adr-0009-resource-fees.md). At height 1 the price is the
genesis `fee_per_unit`. A finalized artifact contains
the header, body, receipts, state-transition records, parent QC and the new
PRECOMMIT QC, all canonically encoded.

The header's `round` is zero based. The sole authorized proposer for `(h,r)`
is selected from the frozen height-`h` set and the authenticated
proposer-priority vector in the state finalized at `h-1`, according to
[ADR 0012](adr-0012-proposer-selection.md). Rank validators by
`priority + weight` descending and validator ID ascending; select index
`r mod active_validator_count`. A valid signature from a different validator
does not authorize the proposal. Finalization advances the weighted primary
priority once, regardless of the successful round; failed rounds do not
change persistent state. Boundary set changes rebase priorities as specified
in ADR 0012. Validators, importers and light clients must verify this rule
from an authenticated set and schedule state, not from the header's claimed
proposer alone.

At height `h`, the active set is the immutable set committed for `h` by the
last finalized boundary. Its checked total weight `W` MUST be positive. If
Byzantine validators control weight `B`, the safety and conditional liveness
fault bound is `3B < W` at every height. This is a weight bound, independent
of validator count, and assumes unforgeable signatures, collision-resistant
commitments and honest validators that durably persist their signed votes and
locks before broadcast. The quorum is exactly `Q = floor(2W/3)+1`, calculated
without overflow as `W - (W-1)/3`. A QC MUST name `Q`, and signed weight
equal to `floor(2W/3)` MUST NOT certify, even when `W` is divisible by three.
Two quorums intersect in weight at least `2Q-W > W/3`, so their intersection
contains honest weight under the bound. Honest weight alone is at least `Q`.
This intersection argument assumes all QCs at a height use the same frozen
set and that honest lock and view-change rules prevent conflicting signatures
across rounds; it does not by itself prove the whole consensus state machine.

The adversary may equivocate, withhold votes and data, and delay, reorder or
partition messages. Safety MUST hold before and after GST without a network
delay bound. Liveness is conditional on partial synchrony: after an unknown
GST, honest-to-honest messages have a finite unknown delay bound, timeouts
eventually exceed it, an honest proposer eventually gets a round, and proposal
data is available. No progress is promised during a partition or when
`3B >= W`. A set change cannot retroactively alter `W` or an old QC. The
[fault-model decision](adr-0002-fault-model-and-quorum.md) records the proof
obligations and implementation boundary. New nodes
MUST obtain a trusted finalized checkpoint no older than
`weak_subjectivity_seconds` in BFT time; they cannot accept a snapshot merely
because a peer supplied a matching manifest.

Time is deterministic. Timestamps are positive Unix seconds in signed `i64`;
zero, noncanonical values and overflow fail closed. For height 1,
`time = checked_add(genesis.time, target_block_seconds)` and no parent QC is
present. At height `h>1`, the child supplies the exact parent PRECOMMIT QC
whose hash is in its header, verified against the frozen set at `h-1`, and
`time = max(checked_add(parent.time, target_block_seconds),
weighted_lower_median(parent_QC.vote_time))`. The median uses the historical
weight of **every** signed voter in that QC, not validator count or a claimed
weight. For total signed weight `S`, sort votes by time and select the first
time whose cumulative weight reaches `ceil(S/2)`; this chooses the lower time
when exactly half the weight is on each side. Duplicate, zero-weight,
non-PRECOMMIT or invalidly signed votes and a vote time before the parent's
header time invalidate the QC time proof. Different valid parent QCs may give
different child times; the child's parent-QC hash makes the choice explicit
and replayable. The [time-model decision](adr-0006-bft-time.md) gives the
proof boundary and required local-clock behavior.

Honest validators sign PRECOMMIT votes with their current wall-clock seconds
only after that clock reaches the block header time. They MUST wait at least
`target_block_seconds` on a local monotonic clock after finalizing a height
before signing at the next height, and persist signed vote times so a wall
clock step backward cannot produce a contradictory timestamp. A future
proposal or vote is held, with bounded buffering, until it is no more than
`max_future_skew_seconds` ahead of local wall time; it is not permanently
invalidated solely by the receiving node's clock. These readiness checks
govern live signing and relay, never historical replay. Under `3B < W` and
honest clock error bounded by the genesis skew parameter, fewer than half of
a valid QC's signed weight is Byzantine, so its median lies within the range
of honest signed times. Consensus timeouts use local monotonic milliseconds
and never enter block hashes. Economic deadlines, evidence age and unbonding
use authenticated BFT header time, not a proposer or receiver's wall clock.

Proposer selection is deterministic weighted round-robin over the frozen set,
with signed integer priority, initial priority zero, validator-ID tie break,
and Tendermint-style update: each round add each validator's weight, select
highest priority, then subtract total weight from the winner. Priorities are
carried across heights; a new validator starts at zero, a removed validator's
priority is discarded, and rounds reset to zero at each height. Priorities are
normalized by subtracting the minimum after each epoch; overflow is invalid.
The schedule is replayable from genesis, including every round and epoch
boundary. A signed
proposal contains header/body plus `valid_round` and the PREVOTE QC for that
round when present. Votes have `chain_id, genesis_hash, height, round,
step:u8, block_id:optional<hash>, validator_id, vote_time:i64, signature`;
steps are PREVOTE=1 and PRECOMMIT=2. Nil is absent `block_id`, not a magic
hash. A QC contains height, round, step, `block_id:optional<hash>`, set root, strictly sorted
unique signed votes and checked total/signed weight, with required weight
exactly `Q` from that historical set. A nil QC may advance a
round but never finalize a block.

For each `(height, round)`, an honest validator signs at most one PREVOTE and
one PRECOMMIT. It persists its intended signed bytes, lock and round to durable
storage **before** broadcasting; recovery reloads them and never signs a
conflicting vote. On a valid proposal, it PREVOTEs the proposal if unlocked,
locked on that block, or shown a valid PREVOTE QC from a strictly higher round
than its lock; otherwise it PREVOTEs nil. On a non-nil PREVOTE QC, it locks
that block and PRECOMMITs it. On a nil QC or timeout it PRECOMMITs nil, then
advances round. A higher-round proposal carrying a valid PREVOTE QC for the
locked block is admissible; a proposer with a lock MUST repropose its lock
unless it has higher-round unlock evidence. A validator never unlocks on a
PRECOMMIT QC for a different block. After a non-nil PRECOMMIT QC, the block is
final, irreversible and must be durably stored with the QC before height
advances. Conflicting finalized blocks at one height are a safety fault; a
node halts signing and preserves both proofs. Fork choice is the greatest
verified finalized height extending the trusted checkpoint; no longest-chain
override exists.

Block rejection codes, checked in order, are `B_ENCODING`, `B_NETWORK`,
`B_PARENT`, `B_HEIGHT`, `B_TIME`, `B_PROPOSER`, `B_SET`, `B_QC`, `B_SIZE`,
`B_TX` (with the transaction code above), `B_EVIDENCE`, `B_ROOT`,
`B_TRANSITION`, `B_SUPPLY`. They mean respectively noncanonical body/header;
wrong identity/version; wrong parent; nonsequential height/epoch;
nonconforming BFT time; wrong or unsigned proposer; set/parameter commitment
mismatch; invalid parent or finality QC; exceeded byte/unit/count limit;
invalid or repeated transaction; invalid/stale/duplicate evidence; any root or
body-size mismatch; nondeterministic/invalid system transition; broken supply
invariant. A validator MUST verify all of them before PREVOTE. Invalid
proposals lead to nil PREVOTE; they are never partly executed.

## 6. Network messages and synchronization

Transport is a length-prefixed frame (`u32` big-endian frame length) containing
one canonical network envelope. Hard limit: 5 MiB frame, 4 MiB complete
envelope including its prefix, plus lower object-specific caps. Zero-length, truncated, oversized
and trailing frames are rejected before decoding. An authenticated session
uses an ephemeral key exchange, a challenge nonce from each peer, and signatures
over the exact [ADR 0008](adr-0008-canonical-binary.md) transcript containing
the complete signed HELLO and CHALLENGE envelopes, their peer IDs, nonces,
ephemeral keys, chain ID and genesis hash, plus the directional traffic-key
commitment and signer role. Nonces MUST be fresh and
replay-guarded. A mismatched network or unsupported version closes the session.
Encryption alone never substitutes for object signature or finality checks.
Peer records use the exact network-bound `SIGN/PEER` preimage in ADR 0008;
their public key must derive the advertised peer ID.

Envelope fields are `chain_id:str, genesis_hash:hash,
type:u16, sender_id:hash, sequence:u64, created_at:i64, ttl_seconds:u32,
payload:bytes, payload_hash:hash, signature:sig`. `payload_hash` is
`H("NET-PAYLOAD", payload)`. Signature covers all preceding fields. The
receiver enforces session sender binding, strictly increasing sequence per
session, bounded TTL (1–300 seconds), size and rate limits. `created_at` and
TTL are relay controls, not consensus time. Duplicate message IDs (hash of
signed envelope) may be dropped without affecting validity. Unknown types are
rejected; no executable payload is accepted through a generic extension tag.

| Tag | Message | Payload and acceptance rule |
| --- | --- | --- |
| 1 HELLO | `peer_id:hash, ephemeral_key:key, nonce:hash, capabilities:u32` | Handshake only. |
| 2 CHALLENGE | `peer_id:hash, ephemeral_key:key, nonce:hash, hello_hash:hash` | Handshake only. |
| 3 AUTH | `hello_hash:hash, challenge_hash:hash, transcript_signature:sig` | Handshake only; binds both identities and ephemeral keys. |
| 4 STATUS | `height:u64, block_id:hash, qc_hash:hash, set_root:hash, protocol_version:u16, rule_set_hash:hash, next_activation_height:u64, next_protocol_version:u16, next_rule_set_hash:hash, supported_versions:list<u16>` | Advisory until proofs verify; zero activation height means none. |
| 5 TX_INV | `ids:list<hash>` (at most 128) | Inventory only. |
| 6 TX_REQUEST | `request_id:u64, ids:list<hash>` (at most 128) | Request only missing IDs. |
| 7 TX_BODY | `request_id:u64, transactions:list<transaction>` (at most 128) | IDs and bodies must match the request; revalidate. |
| 8 PROPOSAL | Canonical signed consensus proposal | Full validation before voting. |
| 9 VOTE | Canonical signed vote | Verify historical set and signature. |
| 10 QC | Canonical QC | Independently verify all votes. |
| 11 FINALIZED | Canonical finalized artifact | Replay before acceptance. |
| 12 BLOCK_RANGE_REQUEST | `request_id:u64, first_height:u64, count:u8` (1–4) | Only contiguous heights. |
| 13 BLOCK_RANGE_RESPONSE | `request_id:u64, artifacts:list<artifact>` (1–4) | Contiguous, requested and within frame cap. |
| 14 CHECKPOINT | `height:u64, block_id:hash, protocol_version:u16, rule_set_hash:hash, state_root:hash, qc:qc` | Match the verified header and QC path from a trusted checkpoint. |
| 15 EVIDENCE_INV | `ids:list<hash>` (at most 64) | Inventory only. |
| 16 EVIDENCE_BODY | `request_id:u64, evidence:list<evidence>` (at most 64) | Verify each item and request binding. |
| 17 SNAPSHOT_MANIFEST | `request_id:u64, height:u64, header_id:hash, protocol_version:u16, rule_set_hash:hash, state_root:hash, chunk_roots:list<hash>` | Version/hash must match the verified header/QC path. |
| 18 SNAPSHOT_CHUNK | `request_id:u64, index:u32, bytes:bytes, proof:list<hash>` (at most 256 KiB data) | Verify chunk and final state root. |
| 19 PEER_EXCHANGE | `peers:list<signed_peer_record>` (at most 32) | Never grants automatic trust. |
| 20 PING | `nonce:u64` | Session liveness only. |
| 21 PONG | `nonce:u64` | Must match an outstanding PING. |

STATUS `supported_versions` is a strictly increasing list of at most 32
distinct nonzero `u16` values. It describes locally installed and verified
codecs only; the sender's claimed finalized version/hash and pending
activation must be checked against a verified chain. With no pending upgrade,
`next_activation_height` is zero and `next_protocol_version` and
`next_rule_set_hash` equal the advertised active pair. These signals never
alter the consensus activation height.

HELLO and CHALLENGE envelopes are each capped at 4096 complete bytes. The
CHALLENGE `hello_hash` is `H("HANDSHAKE-MSG", complete_signed_HELLO_envelope)`;
AUTH carries that same hash and
`H("HANDSHAKE-MSG", complete_signed_CHALLENGE_envelope)`. The AUTH transcript
signature uses the role-bound, key-bound preimage in ADR 0008. A peer rejects
any mismatch before treating the session as authenticated.

Requests MUST be bounded by count and bytes and tied to a session request ID;
unsolicited responses are dropped. A sync response is not authoritative until
every header, QC, body and state transition verifies from a trusted anchor.
Snapshot import requires a recent trusted checkpoint, a verified chain of
finality proofs to the snapshot height, the exact header state root and proofs
for every chunk. The node MUST audit the reconstructed state before signing.
Peer failures are locally classified as `N_FRAME`, `N_HANDSHAKE`, `N_NETWORK`,
`N_AUTH`, `N_REPLAY`, `N_EXPIRED`, `N_RATE`, `N_UNSOLICITED`, `N_PAYLOAD`, or
`N_PROOF`; these diagnostics are not part of block validity.

## 7. Conformance and security gates

Independent implementations MUST agree on canonical bytes and hashes for
every kind, every transaction type, every Merkle edge case, genesis and epoch
boundaries, vote/lock recovery, and each rejection code. Conformance tests
MUST include malformed lengths, duplicate/sorted fields, overflow, unknown
versions, unknown transaction/governance tags, extra payload bytes, nonempty
TEXT actions and attempts to submit code or contract storage, cross-chain
replay, historical set verification, evidence after
unbonding, competing QCs and crash-after-persist/before-broadcast. Fuzz
decoders and compare a second implementation before public testnet. A v1
genesis cannot be published until its concrete parameters, economic schedule,
threat model and test vectors have independent review. Sections here are the
target contract; the existing `nodo/0.x` text codecs, header and runtime do
not satisfy it and MUST NOT be treated as v1-compatible.

Non-normative design references: the [Tendermint consensus
specification](https://github.com/cometbft/cometbft/blob/main/spec/consensus/consensus.md)
describes PREVOTE/PRECOMMIT locks and proof-of-lock changes; the
[CometBFT proposer selection specification](https://github.com/cometbft/cometbft/blob/main/spec/consensus/proposer-selection.md)
describes weighted priority rotation; the [CometBFT light-client
specification](https://github.com/cometbft/cometbft/blob/main/spec/light-client/README.md)
explains trusted checkpoints and validator-set verification. Nodo's v1 rules
above are its own design and still require formal modeling and independent
review; those references are not substitutes for either gate.
