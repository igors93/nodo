# ADR 0008: One versioned binary encoding for v1 consensus objects

**Status:** accepted Phase 1 decision. The v1 primitive writer, domain hash and
ordered Merkle construction have checked reference code and vectors. The
`nodo/0.7` runtime still signs and hashes development text objects; Phase 2.1
must replace those paths atomically before any node can advertise v1.

## Decision

There is exactly one consensus representation: the bytes defined here and in
[protocol v1](protocol-v1.md). A top-level object is the eight-byte prefix
`4e4f444f 0001 KKKK`, where `KKKK` is the big-endian `u16` kind in the closed
1–14 registry. Every nested structured object is `u32(byte_length) ||
schema_bytes`, without another prefix. Fixed hashes, keys and signatures are
32, 32 and 64 raw bytes without a length. A `bytes` or `str` field has one
big-endian `u32` byte length. A list has one big-endian `u32` count, then
length-delimited structured elements or fixed-size primitives. An optional
has a single `00`/`01` flag followed by the value only for `01`. `i64` uses
two's-complement big-endian. No alternative decimal, text, JSON, protobuf,
field map, padding, implicit default or extension encoding is accepted.

For kinds 2, 6, 7 and 14, the signed preimage is the complete top-level
prefix and fields **through but excluding** the signature field; no fake zero
signature or signing timestamp is appended. Handshake and peer-record
signatures use their separate exact encodings below. The signature is Ed25519
over the raw 32-byte
`H(sign_domain, unsigned_preimage)` digest. The domain registry is closed:
`SIGN/TX`, `SIGN/VOTE`, `SIGN/PROPOSAL`, `SIGN/ENVELOPE`,
`SIGN/HANDSHAKE` and `SIGN/PEER`. Object IDs and roots use the distinct domain strings in
protocol v1. A caller must never hash a text rendering, hexadecimal digest,
or object whose typed schema has not passed validation. `H` failures are
fatal. An already-signed object must re-encode byte-for-byte or be rejected.

The maximum **complete top-level size including the prefix** is 4096 bytes
for a header, 1024 for a vote, 65536 for a receipt, 262144 for a transaction
or evidence, 1048576 for genesis, body, QC, validator set or parameter set,
and 4194304 for a proposal, finalized artifact, snapshot or network envelope.
The transport frame is at most 5242880 bytes. Every list also has an absolute
1048576-element bound and may have a lower field limit; count times minimum
element size and length must fit the remaining input before allocating. A
block must satisfy both `max_block_bytes` from genesis and the applicable
container limits. A producer cannot create a block that its finalized
artifact cannot represent.

Decoding checks the prefix, exact version and kind, field and list caps,
two's-complement values, optional flags, UTF-8 NFC, enum ranges, sorted unique
sets, type-specific payload shape and full input consumption. A nested object
must consume **exactly** its declared length. Signature verification and
state-dependent validation follow byte-level checks, never repair or normalize
malformed input. The decoder does not accept unknown fields and never falls
back to the development text parser. Bytes used as identifiers obey the ASCII
alphabet stated for that field. This is a hard fork from `nodo/0.7` text
artifacts: v1 requires a new genesis and explicit activation, not a silent
reinterpretation of old storage.

## Closed schema registry

Field order is fixed. The names below refer to the exact primitive and
structured encodings above; `list<T>` and `optional<T>` use the stated count
and flag. Every reference to another top-level kind uses its **nested**
schema bytes with a length, not another prefix. The full header, transaction,
vote and network message fields and transaction-specific payloads appear in
protocol v1 sections 4–6.

| Kind | Schema fields in order |
| --- | --- |
| 1 genesis | `chain_id:str, genesis_time:i64, parameters:parameter_set, validators:validator_set, initial_accounts:list<key>, initial_lots:list<genesis_lot>, initial_stakes:list<genesis_stake>, issuance_ranges:list<issuance_range>` |
| 2 transaction | `chain_id:str, genesis_hash:hash, type:u8, sender_key:key, nonce:u64, expiry_height:u64, amount:u64, fee:u64, input_lots:list<hash>, payload:bytes, signature:sig` |
| 3 header | `chain_id:str, genesis_hash:hash, height:u64, protocol_version:u16, round:u64, parent_id:hash, time:i64, proposer:validator, validator_set_root:hash, next_validator_set_root:hash, parameter_root:hash, parent_qc_hash:hash, body_root:hash, tx_root:hash, receipt_root:hash, evidence_root:hash, state_root:hash, body_bytes:u32, resource_units:u64` |
| 4 body | `transactions:list<transaction>, evidence:list<evidence>, system_records:list<system_record>` |
| 5 receipt | `tx_or_record_id:hash, units:u64, fee:u64, effect_ids:list<hash>, post_state_root:hash` |
| 6 vote | `chain_id:str, genesis_hash:hash, height:u64, round:u64, step:u8, block_id:optional<hash>, validator_id:validator, vote_time:i64, signature:sig` |
| 7 proposal | `header:header, body:body, valid_round:optional<u64>, valid_prevote_qc:optional<qc>, signature:sig` |
| 8 QC | `height:u64, round:u64, step:u8, block_id:optional<hash>, set_root:hash, votes:list<vote>, total_weight:u64, signed_weight:u64, required_weight:u64` |
| 9 evidence | `kind:u8, first_signed_object:bytes, second_signed_object:bytes` |
| 10 validator set | `entries:list<validator_entry>` |
| 11 parameter set | the 20 exact fields listed below |
| 12 finalized artifact | `header:header, body:body, receipts:list<receipt>, parent_qc:optional<qc>, final_qc:qc` |
| 13 state snapshot | `height:u64, header_id:hash, state_root:hash, state_leaves:list<state_leaf>, finality_path:list<qc>` |
| 14 network envelope | `chain_id:str, genesis_hash:hash, type:u16, sender_id:hash, sequence:u64, created_at:i64, ttl_seconds:u32, payload:bytes, payload_hash:hash, signature:sig` |

Parameter-set fields in exact order and type are
`epoch_length_blocks:u64, target_block_seconds:u64,
min_validator_stake:u64, max_tx_bytes:u32, max_block_bytes:u32,
max_block_units:u64, fee_base:u64, fee_per_unit:u64,
min_proposal_deposit:u64, max_treasury_spend_per_epoch:u64,
treasury_timelock_epochs:u64, max_epoch_churn_basis_points:u16,
activation_delay_epochs:u64, unbonding_seconds:u64,
evidence_max_age_seconds:u64, weak_subjectivity_seconds:u64,
max_future_skew_seconds:u64, double_vote_slash_bps:u16,
double_proposal_slash_bps:u16, jail_seconds:u64`. These fields are present in
both genesis and committed parameter roots, including fields immutable in v1.

`genesis_lot = lot_id:hash, origin_id:hash, owner:account, amount:u64,
status:u8`; `genesis_stake = position_id:hash, owner:account,
validator_id:validator, lot_ids:list<hash>`; and `issuance_range =
first_epoch:u64, last_epoch:u64, units_per_epoch:u64`. A `validator_entry`
is `validator_id:validator, owner:account, consensus_key:key, weight:u64,
status:u8`. A `state_leaf` is `domain:u8, key:bytes, value:bytes`.
`system_record` is `tag:u8, affected_id:hash, previous_value_hash:hash,
new_value:bytes`; tags 1–7 are slash, stake maturity, validator-set change,
governance decision, treasury execution, epoch issuance and parameter change
in that order. Sorted lists use the ordering specified by protocol v1;
duplicate keys, voters, lot IDs or evidence objects are noncanonical.

The QC `block_id` is optional so nil PREVOTE or PRECOMMIT QCs have one
unambiguous encoding; a finality QC MUST have a non-nil ID. `total_weight`,
`signed_weight` and `required_weight` are recomputed from the historical set,
never trusted as independent authority. The envelope has **no second version
field**: the top-level prefix is the signed version. Envelope payload bytes
contain the kind-specific message schema from protocol v1 section 6, not a
second generic envelope. The signed peer record used by PEER_EXCHANGE is
`chain_id:str, genesis_hash:hash, peer_id:hash, endpoint:str,
public_key:key, expires_at:i64, signature:sig`. Its `SIGN/PEER` preimage is
the ASCII bytes `NODO/V1/PEER`, a zero byte, then the same fields through
`expires_at` in the stated order. The signer key must equal `public_key`, and
`peer_id = H("PEER", public_key)`. Endpoint UTF-8 is at most 256 bytes and is
validated before connection; expired or wrong-network records are rejected.

The `SIGN/HANDSHAKE` preimage is the ASCII bytes `NODO/V1/HANDSHAKE`, a zero
byte, then `chain_id:str, genesis_hash:hash, initiator_hello:bytes,
responder_challenge:bytes, session_keys_hash:hash, signer_role:u8` in that
order. `signer_role` is 1 for the initiator and 2 for the responder. The two
`bytes` fields contain complete, valid, signed kind-14 HELLO and CHALLENGE
envelopes, respectively, each at most 4096 bytes. Their sender IDs, payload
peer IDs and network identities must match the named roles and each other;
their nonces and ephemeral keys must be fresh and must produce the negotiated
directional keys. `session_keys_hash = H("HANDSHAKE-KEYS",
initiator_to_responder_key || responder_to_initiator_key)`, with exactly two
32-byte directional traffic keys from the selected authenticated transport
suite. The CHALLENGE `hello_hash` and AUTH `hello_hash`/`challenge_hash` are
`H("HANDSHAKE-MSG", complete_signed_envelope_bytes)` of the appropriate
messages. The AUTH transcript signature covers the preimage under
`SIGN/HANDSHAKE` and is also carried inside its signed kind-14 envelope.
Other handshake and key-schedule requirements remain Phase 5.2 gates.

## Merkle construction and implementation boundary

The v1 Merkle tree preserves order and has no duplicated odd leaf. Each leaf
is `H("MERKLE-LEAF/"+kind, u32(index)||element_bytes)`. For a nonempty
subtree of `n>1` leaves, split at the largest power of two less than `n` and
hash `H("MERKLE-NODE/"+kind, left||right)` recursively. Commit the total
count in `H("MERKLE-ROOT/"+kind, u32(n)||tree_hash)`. Empty roots use
`H("MERKLE-EMPTY/"+kind, empty)`. The allowed `kind` strings are `tx`,
`receipt`, `evidence` and `state`. This prevents a 3-leaf tree from being
confused with a 4-leaf tree formed by repeating its final leaf and supplies a
stable membership-proof tree shape.

The C++ `V1EncodingPrimitives` deliberately supplies only framing, byte caps,
domain hashing and Merkle roots. It never treats an opaque payload as a valid
object. Phase 2.1 must implement typed encoders and strict decoders for **all
14 kinds**, all 13 transaction payloads and all 21 message payloads, then
switch production, voting, hashing, signatures, storage, replay, import and
sync in one incompatible activation. Until that happens `nodo/0.7` remains a
development protocol and cannot claim v1 wire or signature compatibility.
