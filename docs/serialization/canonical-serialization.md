# Canonical Serialization

This page describes the `nodo/0.7` development format. The future v1 binary
contract and its byte vectors are in [protocol v1](../spec/protocol-v1.md) and
[v1 vectors](../spec/vectors-v1.md).

Canonical serialization means the same logical object must always produce the same serialized representation.

This is required because hashes, signatures, state roots, storage checks, and replay validation depend on deterministic bytes or text.

## Core rule

```text
deserialize(serialize(object)).serialize() == serialize(object)
```

If the round trip fails, the input must be rejected.

## Text format rule

Current development serialization uses strict deterministic text for many protocol objects.

Example shape:

```text
ObjectName{fieldA=valueA;fieldB=valueB;fieldC=valueC}
```

Rules:

- object names are case-sensitive;
- field names are case-sensitive;
- fields appear in exact required order;
- semicolon separators are exact;
- no optional spaces;
- no trailing separator;
- no unknown fields;
- no duplicate fields;
- no missing required fields.

## Lists

```text
fieldName=[ObjectA{...},ObjectA{...}]
```

List order must be deterministic. If an object requires sorting, the sorting rule must be part of the specification.

## Numeric values

Numeric values should use canonical base-10 representation unless a specific field explicitly defines another encoding. No ambiguous leading zeros, signs, or whitespace should be accepted.

## Hash and signature safety

A signed object must serialize exactly the same way on every node. A hash commitment must be computed from canonical serialization only.

## Development block header and body

Since `nodo/0.6`, blocks use `Block{...;payload=BlockHeader{...};records=[...]}`. The
header contains height, parent hash, timestamp, record count, an ordered
record Merkle root, state root and receipt root. It contains no record payload.
The block ID is SHA-256 of the exact header text. Each ordered record leaf is
`hashLeaf("NODO_BLOCK_RECORD_0.6:" || decimal_index || ":" || decimal_byte_length || ":" || canonical_record)`;
the existing node hash and odd-leaf duplication rules apply. The record count
is also committed in the header. Decoders reconstruct the block and require
both the recomputed header and full serialization to match byte for byte.
Older record-bearing block headers are rejected. This format is incompatible
with `nodo/0.5` persisted blocks and peers; there is no in-place migration.
`nodo/0.7` also changes PRECOMMIT time admission and rejects `nodo/0.6` peers.
The full v1 header and binary Merkle rules are specified in
[ADR 0005](../spec/adr-0005-compact-block-header.md).

## V1 binary format

The target [v1 protocol](../spec/protocol-v1.md) and
[ADR 0008](../spec/adr-0008-canonical-binary.md) fix one versioned binary
format for every consensus kind. [Object byte vectors](../spec/v1-object-vectors.json)
and C++ primitive tests anchor its framing, hash and Merkle rules. The
`nodo/0.7` development runtime remains text-based on consensus paths; typed
binary codecs and a single incompatible activation are required by Phase 2.1.
