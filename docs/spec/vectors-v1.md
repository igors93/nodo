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
| 1 | genesis | 1867 | `725b772d84248b8580a4212436fd6713061df51da0520edf8abb586e1f42e7b7` |
| 2 | transaction | 249 | `ec2bc81914a4c04ff0d5ff4d682b286c571b2cc106546f9a59c7e7776ef4c8f9` |
| 3 | header | 438 | `7b1dd1b04a3ba635f4696f3372cd9431ada7624a983cb094004ef50ce0cb0bd0` |
| 4 | block body | 265 | `a171e1b9c5fe4b2abc2ab7e4e3ea183fa6068a439f9db5d2e9d87ac1bab1faa7` |
| 5 | receipt | 92 | `f979d17e1799b99a2974570eb655f49c081618996d7adba3ef5bbf21426c88fa` |
| 6 | vote | 202 | `b1b949d3d2e5ea6de8a3c3af95a7c8a756feab1caeb78dbeec84c4a60b0cea74` |
| 7 | proposal | 769 | `7634d435f30a35e321158008bdcf1efc8cea36f021a7d7d7026fe862b6058246` |
| 8 | quorum certificate | 910 | `06a7ab396c4bf3399b0f16f68806d419b846dd0e539aec453155823e5703a138` |
| 9 | evidence | 421 | `e8cffdf54154e9fec06f4ae4ba27142f9a98959c757c52f2e29cf932cd79771a` |
| 10 | validator set | 448 | `32ff9166c940091bb17a58c7d203aed81b966b316e1aee3b47d20d12a46fa381` |
| 11 | parameter set | 142 | `ffb5a5aacbcdc6ef18f8eeb60936315243eaa3af40e989ce6ad8fdc9fbd8e0b4` |
| 12 | finalized artifact | 1702 | `282e2b10e5989e2487aad0fc3e022cbdb7ceea6fc5c98d508a2773fa2108e80d` |
| 13 | state snapshot | 1046 | `c1bb6e11e97f0fe823c9d58eca4e1843a52366de91be10ead5579852b985b4f0` |
| 14 | network envelope | 404 | `d8c303c870827a5bdb6528675e339d58b1a0298c3158112caf413910659c5d77` |

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
