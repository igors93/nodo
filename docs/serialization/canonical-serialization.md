# Canonical Serialization

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

## Future binary format

A future production binary format may replace development text serialization, but it must preserve deterministic ordering, strict field definitions, round-trip validation, and compatibility rules.
