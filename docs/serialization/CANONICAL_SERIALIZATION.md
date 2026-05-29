# Nodo Canonical Serialization Rules

Status: Development Specification  
Version: NODO-CANONICAL-SERIALIZATION-RULES-V1

## Purpose

This document defines the canonical serialization rules used by Nodo during the current development phase.

Canonical serialization means that the same logical object must always produce the exact same serialized bytes or text. This is mandatory for a blockchain because hashes, signatures, storage checks, and state reconstruction depend on deterministic data.

In simple terms:

```text
same object
        ↓
same serialization
        ↓
same hash
        ↓
same validation result
```

If two valid nodes serialize the same object differently, they may calculate different hashes and disagree about the blockchain.

## Current Scope

These rules apply to the current deterministic text serialization used by Nodo.

Covered object families:

- monetary records;
- public transactions;
- ledger records;
- blocks;
- privacy commitments;
- privacy nullifiers;
- private accounting records;
- chain manifests;
- block storage indexes;
- block snapshot headers;
- persisted block snapshots.

This document does not define the final production binary format. It defines the mandatory behavior for the current text format and the security expectations for future formats.

## Non-Goals

This document does not specify:

- production cryptographic algorithms;
- final binary encoding;
- network message framing;
- mempool wire protocol;
- peer-to-peer synchronization format;
- zero-knowledge proof encoding;
- wallet key storage format.

Those areas must be specified separately.

## Core Rule

Every serializable object must follow this rule:

```text
deserialize(serialize(object)).serialize() == serialize(object)
```

If this round-trip check fails, the input must be rejected.

## Canonical Text Format

### Object Shape

Each object must use this shape:

```text
ObjectName{fieldA=valueA;fieldB=valueB;fieldC=valueC}
```

Rules:

- the object name is case-sensitive;
- the opening token must be exactly `ObjectName{`;
- the closing token must be exactly `}`;
- field names are case-sensitive;
- fields must appear in the exact required order;
- fields must be separated by exactly one semicolon `;`;
- no optional spaces are allowed;
- no trailing semicolon is allowed before `}`;
- no unknown fields are allowed;
- no duplicated fields are allowed;
- no missing required fields are allowed.

Valid:

```text
MintRecord{id=mint_001;recipient=igor;amountRaw=100000000000;reason=GENESIS_ALLOCATION;epoch=0;sourceBlockIndex=0;sourceBlockHash=GENESIS;timestamp=1700000000}
```

Invalid:

```text
MintRecord{ id=mint_001; recipient=igor }
MintRecord{recipient=igor;id=mint_001}
MintRecord{id=mint_001;recipient=igor;recipient=ana}
MintRecord{id=mint_001;recipient=igor;unknown=value}
```

### Lists

Lists must use this shape:

```text
fieldName=[ObjectA{...},ObjectA{...}]
```

Rules:

- list opening must be exactly `[`;
- list closing must be exactly `]`;
- items must be separated by exactly one comma `,`;
- no space is allowed after commas;
- list order is meaningful and must be preserved;
- empty lists are allowed only when the object rules explicitly allow them;
- nested object splitting must be brace-aware, not simple comma splitting.

Valid:

```text
outputCommitments=[PrivacyCommitment{...},PrivacyCommitment{...}]
```

Invalid:

```text
outputCommitments=[PrivacyCommitment{...}, PrivacyCommitment{...}]
outputCommitments=[PrivacyCommitment{...},]
outputCommitments=[,PrivacyCommitment{...}]
```

## Primitive Values

### Strings

String fields are written directly without quotes.

Rules:

- strings must not contain field separators such as `;`, `{`, `}`, `[`, `]`, or unescaped commas when inside lists;
- identifiers should use safe characters only;
- file names must never contain `/`, `\`, `..`, or path traversal markers;
- codec-specific validation may be stricter.

### Integers

Integer fields must be base-10 ASCII digits.

Rules:

- no leading `+`;
- no decimal points;
- no exponent notation;
- no underscores;
- no whitespace;
- unsigned fields must reject negative values;
- signed fields may accept `-` only when the domain allows negative values;
- timestamps must be positive unless a future rule explicitly says otherwise.

Valid:

```text
0
1
1700000000
100000000000
```

Invalid:

```text
+1
01
1.0
1e3
1_000
 100
100 
```

### Amounts

Amounts must be serialized as raw integer units, never as floating-point values.

Required field style:

```text
amountRaw=100000000000
feeRaw=100000
publicSupplyAmountRaw=100000000000
```

Rules:

- no floating-point serialization;
- no locale formatting;
- no commas;
- no decimal separator;
- no unit suffix inside serialized fields.

Valid:

```text
amountRaw=2500000000
```

Invalid:

```text
amount=25.00000000 NODO
amountRaw=2,500,000,000
amountRaw=25.0
```

### Hashes

Hash-like fields must be hexadecimal unless a specific rule allows a symbolic value.

Rules:

- non-empty;
- hexadecimal characters only: `0-9`, `a-f`, `A-F`;
- symbolic `GENESIS` is allowed only for previous-hash or source fields where explicitly defined;
- production formats should prefer lowercase canonical hex.

Examples of hash-like fields:

- `hash`;
- `blockHash`;
- `previousHash`;
- `payloadHash`;
- `commitmentHash`;
- `nullifierHash`;
- `contextHash`;
- `proofHash`;
- `manifestHash`;
- `indexHash`;
- `calculatedHash`.

### Timestamps

Timestamps are signed 64-bit integer fields but must currently be positive.

Rules:

- Unix-style integer seconds in the current development format;
- no fractional seconds;
- no timezone text;
- no ISO date strings;
- must be greater than zero.

Valid:

```text
timestamp=1700000000
createdAt=1700000000
```

Invalid:

```text
timestamp=0
timestamp=-1
timestamp=2026-01-01T00:00:00Z
```

## Codec Responsibilities

Every codec must do all of the following:

1. Confirm that the serialized object starts with the expected object prefix.
2. Extract required fields using deterministic field boundaries.
3. Parse numeric fields strictly.
4. Reject malformed, missing, duplicated, or unknown data whenever possible.
5. Reconstruct the target object.
6. Run the object's own validation.
7. Run a canonical round-trip check.
8. Throw a clear exception if input is invalid.

Required round-trip check:

```cpp
if (rebuilt.serialize() != serialized) {
    throw std::logic_error("Round-trip serialization mismatch.");
}
```

## Current Codec Boundaries

The following object families should be parsed through codec boundaries:

- `MintRecordCodec`;
- `PrivacyCommitmentCodec`;
- `PrivacyNullifierCodec`;
- `PrivateAccountingRecordCodec`;
- `LedgerRecordCodec`;
- `BlockCodec`;
- `ChainManifestCodec`;
- `BlockStorageIndexCodec`;
- `BlockSnapshotHeaderCodec`.

Legacy ad-hoc parsing should not be added back into domain classes or tests.

## Security Rules

### Rule 1: Storage Input Is Untrusted

Anything loaded from disk must be treated as attacker-controlled input until validated.

This includes:

- `chain_manifest.nodo`;
- `block_index.nodo`;
- files under `data/blocks/`.

### Rule 2: Parsing Must Not Mutate State

Parsing only rebuilds objects. It must not directly modify ledger state, account balances, coin lots, nullifier sets, or blockchain state.

State changes must happen only after accepted blockchain validation and state reconstruction.

### Rule 3: Round-Trip Mismatch Means Rejection

If an object can be parsed but serializes back differently, the input is not canonical and must be rejected.

### Rule 4: Hashes Must Be Recomputed

Stored hashes must never be trusted only because they are present.

Relevant hashes must be recomputed and compared.

### Rule 5: Lists Must Preserve Order

List order must be preserved because changing order can change object hashes, block hashes, and validation results.

### Rule 6: Path-Like Values Must Be Restricted

Storage metadata must never allow path traversal.

Invalid file names include:

```text
../block_0_hash.nodo
blocks/block_0_hash.nodo
..\block_0_hash.nodo
```

### Rule 7: Development Serialization Must Stay Clearly Marked

Current text serialization is useful for development and auditing. It is not the final production format.

Any production migration must be explicit and versioned.

## Object-Specific Canonical Shapes

### MintRecord

```text
MintRecord{id=<id>;recipient=<address>;amountRaw=<raw>;reason=<reason>;epoch=<epoch>;sourceBlockIndex=<index>;sourceBlockHash=<hash-or-GENESIS>;timestamp=<timestamp>}
```

### PrivacyCommitment

```text
PrivacyCommitment{id=<id>;type=<type>;commitmentHash=<hash>;ownerHint=<hint>;sourceReference=<reference>;timestamp=<timestamp>}
```

### PrivacyNullifier

```text
PrivacyNullifier{id=<id>;type=<type>;nullifierHash=<hash>;contextHash=<hash>;createdAt=<timestamp>}
```

### PrivateAccountingRecord

```text
PrivateAccountingRecord{id=<id>;type=<type>;supplyEffect=<effect>;publicSupplyAmountRaw=<raw>;auditReference=<reference>;proofHash=<hash>;timestamp=<timestamp>;inputNullifiers=[...];outputCommitments=[...]}
```

### LedgerRecord

```text
LedgerRecord{id=<id>;type=<type>;sourceId=<id>;payloadHash=<hash>;timestamp=<timestamp>;payload=<payload-object>}
```

### Block

```text
Block{index=<index>;previousHash=<hash-or-GENESIS>;hash=<hash>;timestamp=<timestamp>;recordCount=<count>;payload=BlockHeader{index=<index>;previousHash=<hash-or-GENESIS>;timestamp=<timestamp>;records=[...]}}
```

### ChainManifest

```text
ChainManifest{chainVersion=<version>;blockCount=<count>;genesisHash=<hash>;latestHash=<hash>;createdAt=<timestamp>;manifestHash=<hash>}
```

### BlockStorageIndex

```text
BlockStorageIndex{indexVersion=<version>;chainManifestHash=<hash>;blockCount=<count>;createdAt=<timestamp>;indexHash=<hash>;entries=[...]}
```

### BlockIndexEntry

```text
BlockIndexEntry{blockIndex=<index>;blockHash=<hash>;fileName=block_<index>_<hash>.nodo}
```

### BlockSnapshotHeader

```text
BlockSnapshotHeader{blockIndex=<index>;previousHash=<hash-or-GENESIS>;blockHash=<hash>;timestamp=<timestamp>;recordCount=<count>;calculatedHash=<hash>}
```

## Versioning Rules

Future serialization changes must be versioned.

Required behavior:

- old versions must not be silently interpreted as new versions;
- new versions must not be silently interpreted as old versions;
- codecs must reject unsupported versions;
- migration code must be explicit and tested.

## Binary Encoding Evaluation Gate

Nodo should not move to binary canonical encoding until these conditions are met:

- all current text codecs have tests;
- canonical rejection tests exist for malformed input;
- storage corruption tests cover manifest, index, and block snapshots;
- production hash strategy is selected;
- real signature provider boundary exists;
- object versioning rules are stable.

## Contributor Checklist

Before adding or changing serialization logic, verify:

- [ ] Is there exactly one codec boundary for this object?
- [ ] Are fields emitted in one fixed order?
- [ ] Are numeric fields parsed strictly?
- [ ] Are hashes recomputed where needed?
- [ ] Does the codec reject malformed input?
- [ ] Does the codec enforce round-trip equality?
- [ ] Is there a test for the valid path?
- [ ] Is there a test for tampered or malformed input?
- [ ] Does the change avoid hidden state mutation during parsing?
- [ ] Is the development-only format clearly marked?

## Summary

Nodo serialization must be deterministic, strict, and suspicious by default.

The current text format is acceptable for development because it is readable and testable. However, every parser must behave as if the input is hostile.

The long-term goal is to preserve the same strict behavior when Nodo eventually evaluates a binary canonical format.
