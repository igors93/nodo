# Nodo v1 canonical encoding vectors

These vectors are normative for the primitive encoding and domain-separated
hash rules in [protocol v1](protocol-v1.md). Hex is lowercase; spaces in the
examples are visual separators and are **not** serialized. The transaction
example is an **encoding fixture only**: its synthetic key/signature and fee
do not make it a valid transaction. Valid signed-object vectors for every kind
remain part of roadmap item 1.9 and the Phase 1 exit gate.

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
