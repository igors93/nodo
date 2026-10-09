# ADR 0005: Compact, body-independent block header

**Status:** accepted Phase 1 design decision. The incompatible `nodo/0.6`
development split is implemented; canonical v1 bytes and authenticated
header-only sync remain Phase 2 and Phase 5 gates.

## Decision

A block ID MUST identify a bounded-size, canonical header, independently of
the size of its body. The v1 header is the ordered binary schema in
[protocol v1, section 5](protocol-v1.md#5-blocks-consensus-and-finality).
It commits to chain ID and genesis hash, height, active and next rule version
and rule-set hash under [ADR 0010](adr-0010-protocol-upgrades.md), consensus
round and parent ID, BFT time and proposer, the active and next validator-set
roots, the parameter root and derived base fee per unit, the parent PRECOMMIT
QC hash, the complete body root and length, transaction, receipt and evidence
roots, the resulting state
root, and resource units. The block ID is `H("BLOCK", canonical_header_bytes)`,
where those bytes include the complete eight-byte v1 top-level header prefix.
No body record, signature or QC bytes are embedded in the header.
The derived fee is recomputed from the parent's authenticated resource usage
and historical limit under [ADR 0009](adr-0009-resource-fees.md).

`body_root = H("BODY", canonical_body_bytes)` binds every byte of the complete
top-level body, including its eight-byte v1 prefix and ordered
system-transition records. Indexed Merkle leaves bind every
transaction, receipt and evidence item to its position. Evidence is sorted
and unique before commitment; transaction order is execution order. The
validator-set and parameter roots use the canonical binary snapshots selected
for that height, not mutable current-node objects. A boundary block commits
the next set before validators vote on the block. The next header MUST use the
set committed for its height. At an upgrade boundary the old-rule finality
QC authenticates the next version and rule-set hash before the new codec
takes effect. These rules prevent a valid QC from being
reinterpreted under another set, chain or parameter schedule.
The resulting state root also commits the next-height proposer-priority
vector under [ADR 0012](adr-0012-proposer-selection.md). The header's
proposer ID is checked against the prior finalized state's vector and its
round, rather than trusted as a free choice.

At height 0, parent ID and parent QC hash are all zero and genesis validation
uses the genesis-specific body and state rules. Height 1 has the genesis ID
as parent, zero parent QC hash and the genesis validator-set trust anchor.
At every later height, `parent_id` equals the previously finalized block ID
and `parent_qc_hash` equals the domain hash of its verified canonical
PRECOMMIT QC. A proposer supplies the QC as an artifact beside the header;
verifiers MUST check it before accepting the child. The child's own QC is
likewise separate and certifies its header ID.

Verification is fail-closed. Decoders enforce exact version, field order,
lengths, size limits, unique elements and full input consumption before
allocation or hashing. A full node recomputes body length/root, list roots,
receipts, evidence validity, active/next set roots, parameters, resource use
and post-state root before voting or committing. A light client starts from
an authenticated recent checkpoint, verifies the header and QC sequence
against committed historical sets, and accepts inclusion proofs only against
the corresponding committed root. A header hash alone does not establish
finality or data availability.

## Development migration and boundaries

`nodo/0.6` removes records from `Block::headerPayload` and moves them into
the serialized block body. Its compact text header commits the height,
parent hash, timestamp, record count, ordered record Merkle root, receipt
root and state root. Record leaves include index and byte length, so changing
execution order changes the block ID. Canonical full-block decoding compares
the recomputed header and serialization byte for byte. Snapshot readers can
hash and validate header metadata without embedding the body. Development
transaction proofs bind the canonical record bytes, its index and ID to the
root in that header. This is an
incompatible wire and persisted-block change; `nodo/0.5` artifacts require
a fresh development genesis.

The development header does **not** yet contain the v1 chain, proposer,
validator-set, parameter, parent-QC or evidence commitments. Its text hash
and BLS-based QC are also not v1. Phase 2.1 must replace the development
codec and hash domains, Phase 4.8 must bind consensus validation and epoch
transitions to the new fields, and Phase 5.6 must deliver bounded,
checkpoint-anchored transition and inclusion proofs. Until then,
`light_getHeaders` is a diagnostic development endpoint, not an independent
trust anchor.
