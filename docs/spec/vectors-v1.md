# Nodo v1 canonical encoding vectors

These vectors are normative for the primitive encoding and domain-separated
hash rules in [protocol v1](protocol-v1.md). Hex is lowercase; spaces in the
examples are visual separators and are **not** serialized. The transaction
example is an **encoding fixture only**: its synthetic key/signature and fee
do not make it a valid transaction. The separate
[14-kind object vectors](v1-object-vectors.json) contain full encoded bytes for
every top-level consensus kind, including verifiable Ed25519 signatures on a
transaction, vote, proposal and envelope. They are encoding and signature
fixtures, not a claim that their block executes against a valid chain state.
The same file also includes a role-bound handshake transcript, a signed peer
record and verifiable signatures for both. Its synthetic traffic keys test
the preimage only, not a key exchange implementation.
Run `python scripts/generate_v1_object_vectors.py --check` from the repository
root to independently regenerate and verify them (requires the Python
`cryptography` package).

| Input | Canonical bytes / SHA-256 digest |
| --- | --- |
| `u16(1)` | `0001` |
| `u32(4) || "test"` | `0000000474657374` |
| `u64(1)` | `0000000000000001` |
| `bool(false) || bool(true)` | `0001` |
| `H("TXID", empty)` | `b94f996f5d769d8723b6f8f256e08ff0b0f8249f2d153a0be5953dc6218488f4` |
| `H("BLOCK", empty)` | `74c2d93e10f9a8a0f70d4a32033aa2dce65cf47f392da7d8e6c1d97f763aac4b` |
| Empty Merkle root, kind `tx` | `cc9ac246c73fab960612cfc09e7d9a25f81890ddc55c5040459e1953ddd72de8` |
| Leaf 0, kind `tx`, element `00ff` | `5a0c82940057d0ee92a6938474251239e40f2d22f130e0e1073808cd5e420c8e` |
| Root, kind `tx`, one element `00ff` | `7cfa61cc1b253b6ae110f12dc45e451386374f8d68c755bcdc9f978dbb278efc` |
| Root, kind `tx`, elements `a,b,c` | `59e0753959cd18d73dc9bd76ab0536e14772c4e272a9a926fffa4a2e24513de6` |
| Root, kind `tx`, elements `a,b,c,c` | `7821b4c87bceb6815630fd7eb197eab0073ee8cb67c772bceee50c84514e820a` |
| `H("NET-PAYLOAD", 00ff)` | `ec5f3ba6c966269c84867fb47c3df1d96b59f5d557cd4e8dde5c923b40aa957f` |

The following fixture encodes transaction kind 2 with chain ID `test`, zero
genesis hash, TRANSFER type 1, sender key `11` repeated 32 times, nonce 1,
expiry height 2, amount 1, fee 100, one input lot `22` repeated 32 times,
recipient `33` repeated 32 times, and signature `44` repeated 64 times. The
payload length is 32 and the total serialized length is 249 bytes.

```text
4e4f444f000100020000000474657374
0000000000000000000000000000000000000000000000000000000000000000
01
1111111111111111111111111111111111111111111111111111111111111111
0000000000000001
0000000000000002
0000000000000001
0000000000000064
00000001
2222222222222222222222222222222222222222222222222222222222222222
00000020
3333333333333333333333333333333333333333333333333333333333333333
44444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444
```

`H("TXID", fixture_bytes)` is
`619f04e5ef2476b5071a2eaa135c7766b58deaef0cc8dddc5864cb6a407fd743`.

Decoders MUST reject the same fixture if its version becomes `0002`, the
chain-ID length becomes `00000005` without adding a byte, the input count
becomes 2 without a second input, the signature loses a byte, or any byte is
appended. A duplicate input ID is rejected by the transaction validity layer.

## Complete top-level object byte vectors

The linked JSON file supplies each complete `bytes_hex`, size, kind and raw
SHA-256 checksum. The checksum below is for transfer integrity; consensus IDs
still use the domain-separated `H` rule above. The four signed kinds use the
public key and signatures recorded in that file. Nested objects have the same
schema bytes with a `u32` length and without a second eight-byte prefix.

| Kind | Object | Bytes | Raw SHA-256 |
| ---: | --- | ---: | --- |
| 1 | genesis | 1907 | `f3a85a24c4adf46dc25c52f7cb3752defde18b7630dd6c5ea81851a0e4546d96` |
| 2 | transaction | 249 | `83cebde2e915140b0171a11a465a57126bfee3fa8bf5d60ce69db97bbe84871e` |
| 3 | header | 512 | `fd4b97ec6cc44dcab6112e857a1c64ce73ad0b5a3b6a89195cac1d61cdc59142` |
| 4 | block body | 265 | `af8b065d5f6ec46e66e169b63e2b7a0ac22ed8f780aa6e7b32bb43303208a9dc` |
| 5 | receipt | 92 | `a3fc2b10e923af0dfca8d34e8e018f59a55885a2ee416319d0003015954ee473` |
| 6 | vote | 202 | `002ba0267a035340b4b8388170b3df0020962ce4af5a7586a20802561fb0310e` |
| 7 | proposal | 843 | `e1df9bba8f80c44cc2017d0088927fae74acc0bf9c97db67bb5e6a229d867d83` |
| 8 | quorum certificate | 910 | `9cd98fbf0673798c7e55b00baa09a6bc4da3fb74d589b63fcbc3cfa00bb1f4f0` |
| 9 | evidence | 421 | `6ee7fe06ce3e5a8efbd40cb050ab014f61c9370affac13871ee3280040e8cc77` |
| 10 | validator set | 448 | `32ff9166c940091bb17a58c7d203aed81b966b316e1aee3b47d20d12a46fa381` |
| 11 | parameter set | 150 | `1414a5633f1f08be6dd72080df277b1f4b606380c9216de38f715e85bbd56cee` |
| 12 | finalized artifact | 1776 | `77417e00a043b5376cc6dbe8abfd37237c336541f091b60d515f2b1e2aad42ea` |
| 13 | state snapshot | 1080 | `90642292882a97497415e2a72f0c33f97b70fef70bf1b77b8a59fdb5e58b0c6f` |
| 14 | network envelope | 404 | `f394cfceacb7e0f26fee8c0645a4d10c58f1c87aaf4bd11b416303b8b173edcc` |

## Protocol upgrade vectors

The object fixture's genesis `rule_set_hash` is
`cf4744102a1a854a942adfce97ca5a05c65d56c97c4fd73f3ca54d6d2d722e8a`:
`H("RULESET", u16(1) || 91*32 || 92*32 || 00*32)`. The bundle and
conformance digests here are synthetic encoding fixtures, not published rule
artifacts. For a version-2 action using `11*32` bundle digest, `22*32`
vector digest and no migration, `rule_set_hash` is
`6deaf29d503086905c6e1739276667cc20b29a0b9802bbb2ff935fbb81f3c851`.

With genesis `L=10`, an upgrade executed at height 12 is in epoch 1 and
cannot activate before height 61 (epoch 6). Heights 21–60 are the four
complete intervening epochs. Header 59 commits current/next version 1;
header 60 commits current version 1 and next version 2 with the exact new
rule hash; header 61 uses version 2. Height 60, 51, a skipped version or a
second pending action is invalid. If a governed cancellation executes at
height 60 before header finalization, header 60 instead commits version 1
for both current and next, and height 61 remains version 1. Cancellation at
height 61 is too late. These are consensus arithmetic fixtures independent
of governance signatures and the synthetic object bytes above.

## Weighted proposer vectors

The [v1 proposer decision](adr-0012-proposer-selection.md) uses a frozen,
ID-sorted set with IDs `01*32`, `02*32`, `03*32`, `04*32`, weights
`[1,3,1,1]`, total weight 6 and all initial priorities zero. The round-zero
primaries at heights 1–6 are `[02,01,02,03,04,02]`; the assigned counts are
exactly `[1,3,1,1]`. At height 1, the sorted round fallback is
`[02,01,03,04]`; round 4 returns `02`, and round `u64::MAX` returns `04`.
Finalizing height 1 in round 2 with signer `03` produces the same next
priorities as round 0 with signer `02`: `[1,-3,1,1]`. A wrong signer is
invalid. The next state's canonical domain-12 value has 260 bytes with
set root `aa*32`, last finalized height 1, total weight 6, count 4,
each entry length 48, sorted
IDs and signed 16-byte priorities. Its
`H("STATE-VALUE", exact_value_bytes)` is
`a178e9d58e90af1ced3dda5bd114d00b1cc836570fa0032792bb36a8b319768a`.

With epoch length 2, replace ID `04` by new ID `05` at the end of height 2.
The old primary advances first; carried IDs retain their priorities; ID
`05` begins below the minimum, and the centered next vector is
`[-1,3,5,-7]`. The next primary is `02`. A set change at height 1 is
invalid. Over seven stable heights, a weight-4 ID receives four primary
slots; splitting its stake into two weight-2 IDs still gives the pair four
slots. These are schedule and byte vectors, not complete executable blocks.

## Fixed-function payload vectors

These check only the closed v1 payload and amount shapes of
[ADR 0011](adr-0011-fixed-function-v1.md). The bytes shown are the contents
of a transaction's `payload:bytes`, after its outer `u32` length. State,
signatures and fees still require independent validation.

| Type | Amount | Payload contents | Shape result |
| ---: | ---: | --- | --- |
| 1 TRANSFER | 1 | 32 zero bytes | Valid shape |
| 1 TRANSFER | 1 | 33 zero bytes | Reject extra byte |
| 2 BURN | 1 | Empty | Valid shape |
| 2 BURN | 0 | Empty | Reject amount |
| 7 VALIDATOR_REGISTER | 0 | 64 zero bytes | Valid shape |
| 11 GOVERNANCE_PROPOSE | 1 | `03 || 00*32 || 00000000 || 00*8` (45 bytes) | Valid TEXT shape; no execution |
| 11 GOVERNANCE_PROPOSE | 1 | `03 || 00*32 || 00000004 || 0061736d || 00*8` | Reject nonempty TEXT action, including WASM magic |
| 11 GOVERNANCE_PROPOSE | 1 | `04 || 00*32 || 0000006a || 00*106 || 00*8` (151 bytes) | Valid upgrade-action shape only; schedule and digest checks still apply |
| 11 GOVERNANCE_PROPOSE | 1 | Same as above with action length `00000069` | Reject length mismatch |
| 12 GOVERNANCE_VOTE | 0 | `00*32 || 04 || 00*32` | Reject unknown choice |
| 14 | 0 | `0061736d` | Reject unknown transaction type |

The only action content lengths are 17 bytes for parameter change, 40 for
treasury spend, 0 for text, 106 for protocol upgrade and 40 for cancellation.
Parameter tag 8 and governance kind 6 are invalid. These are shape vectors;
zero-valued IDs or hashes in them do not claim semantic validity.

## Resource and fee vectors

The [v1 fee decision](adr-0009-resource-fees.md) uses complete top-level
transaction and receipt lengths. These are arithmetic fixtures, independent
of the encoding-only transaction above. Reference parameters are
`max_tx_bytes=262144`, `max_block_bytes=1048576`, `max_tx_units=524288`,
`max_block_units=5000000`, `fee_base=1`, `fee_per_unit=1`.

| Input | Required result |
| --- | ---: |
| TRANSFER, `tx_bytes=249`, `receipt_bytes=92`, `I=1`, `O=1`, `E=0` | `tx_units=2005` |
| Same transaction at `base_fee_per_unit=1` | `min_fee=2006` |
| Evidence with `complete_bytes=421` | `evidence_units=2981` |
| System record with `nested_bytes=100`, `receipt_bytes=156`, `E=2` | `system_units=1408` |
| Mandatory proposer record with `nested_bytes=105`, `receipt_bytes=124`, `E=1` | `system_units=1317` |
| Mandatory liveness record with the same byte sizes and 4 active validators | `liveness_units=1829` |
| Same liveness record with 9619 active validators | `liveness_units=1232549` |
| Arithmetic sum of the transfer, evidence and generic system record | `resource_units=6394` |
| Same work plus both mandatory per-height records with 4 validators | `resource_units=9540` |
| Parent `base_fee=100`, `M=5000000`, `U=2500000`, child floor 1 | Next base fee `100` |
| Same parent with `U=5000000` | Next base fee `112` |
| Same parent with `U=0` | Next base fee `88` |
| Parent `base_fee=1`, `U=2500001` | Next base fee `2` |
| Parent `base_fee=100`, `U=0`, child floor `200` | Next base fee `200` |

The user transaction budget is `3750000` units for this parameter set. A sum
of `3750001` user units is invalid even when the whole block is below
`5000000`; adding evidence or system work must also keep the full sum at or
below `5000000`. All multiplication and overflow checks precede comparison
with an offered u64 fee.

## Liveness accountability vectors

For a synthetic four-validator set sorted by 32-byte ID suffixes `01` to
`04`, weights `[4,3,2,1]`, a set root with suffix `63`, epoch 0 and zero
counters, the exact tag-13 state value is 276 bytes. Its
`H("STATE-VALUE", value_bytes)` is
`6a87da18306fbcb88a69a4c705acadc873460eb4cbe5eb591a138dfbfa0196b0`.
Each entry has a four-byte `00000030` length followed by ID, `weight:u64`
and zero `signed_heights:u64`.

With `L=288`, height 1 has no parent QC. Heights 2 through 288 count
certified parent QCs 1 through 287. If every QC has only signers `01` and
`02`, their weight is exactly the required `Q=7` of `W=10`, and the
boundary assessments are signed heights `[287,287,0,0]`. The last two IDs
are inactivity candidates because `4*0 < 3*287`; the first two pass. The
boundary QC at height 288 is ignored by height 289's new window. A duplicate
signer, weight 6, wrong set root or wrong parent height is invalid and does
not change state. This vector establishes the candidate calculation, not an
automatic slash or unbounded jail.

For `L=289`, the window has 288 QCs: 215 included signatures fail the
75% threshold, while exactly 216 pass.

## Strict weighted quorum vectors

For every positive total weight `W`, the required weight is
`Q = floor(2W/3)+1 = W-(W-1)/3`. The only encoded fraction is `(2,3)`.
`W=0` and every other fraction are invalid. The examples below are consensus
arithmetic fixtures, independent of validator identity or signature encoding.

| Total weight `W` | Required `Q` | Signed weight just below `Q` |
| ---: | ---: | ---: |
| 1 | 1 | 0 |
| 2 | 2 | 1 |
| 3 | 3 | 2 |
| 4 | 3 | 2 |
| 5 | 4 | 3 |
| 6 | 5 | 4 |
| 10 | 7 | 6 |
| 18446744073709551615 | 12297829382473034411 | 12297829382473034410 |

The third column MUST NOT certify. In particular, 2 of 3 and 4 of 6 exact
weight units are insufficient despite being exactly two thirds of the total.
