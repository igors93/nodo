"""Regenerate the v1 binary fixture vectors; requires `cryptography`.

These are encoding and signature fixtures, not valid executed chain state.
They independently exercise all 14 top-level schemas. The node must not use
this script as a decoder or substitute it for Phase 2 typed validation.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
from pathlib import Path

from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric.ed25519 import (
    Ed25519PrivateKey, Ed25519PublicKey,
)

ROOT = Path(__file__).resolve().parent.parent
OUTPUT = ROOT / "docs" / "spec" / "v1-object-vectors.json"
ZERO = bytes(32)
KINDS = (
    "genesis", "transaction", "header", "block_body", "receipt", "vote",
    "proposal", "quorum_certificate", "evidence", "validator_set",
    "parameter_set", "finalized_artifact", "state_snapshot", "network_envelope",
)


def h(domain: str, data: bytes) -> bytes:
    return hashlib.sha256(b"NODO/V1/" + domain.encode("ascii") + b"\0" + data).digest()


def u8(value: int) -> bytes:
    return value.to_bytes(1, "big")


def u16(value: int) -> bytes:
    return value.to_bytes(2, "big")


def u32(value: int) -> bytes:
    return value.to_bytes(4, "big")


def u64(value: int) -> bytes:
    return value.to_bytes(8, "big")


def i64(value: int) -> bytes:
    return value.to_bytes(8, "big", signed=True)


def blob(data: bytes) -> bytes:
    return u32(len(data)) + data


def nested(payload: bytes) -> bytes:
    return blob(payload)


def items(values: list[bytes], *, structured: bool = False) -> bytes:
    return u32(len(values)) + b"".join(nested(v) if structured else v for v in values)


def obj(kind: int, payload: bytes) -> bytes:
    return b"NODO" + u16(1) + u16(kind) + payload


def pubkey(private: Ed25519PrivateKey) -> bytes:
    return private.public_key().public_bytes(
        serialization.Encoding.Raw, serialization.PublicFormat.Raw
    )


def signed(kind: int, domain: str, unsigned_payload: bytes,
           private: Ed25519PrivateKey) -> tuple[bytes, bytes]:
    unsigned = obj(kind, unsigned_payload)
    signature = private.sign(h(domain, unsigned))
    return unsigned_payload + signature, signature


def merkle(kind: str, values: list[bytes]) -> bytes:
    if not values:
        return h("MERKLE-EMPTY/" + kind, b"")
    leaves = [h("MERKLE-LEAF/" + kind, u32(i) + value)
              for i, value in enumerate(values)]

    def tree(part: list[bytes]) -> bytes:
        if len(part) == 1:
            return part[0]
        split = 1 << ((len(part) - 1).bit_length() - 1)
        return h("MERKLE-NODE/" + kind,
                 tree(part[:split]) + tree(part[split:]))

    return h("MERKLE-ROOT/" + kind, u32(len(values)) + tree(leaves))


def make_vectors() -> dict[str, object]:
    keys = [Ed25519PrivateKey.from_private_bytes(bytes([n]) * 32)
            for n in range(1, 5)]
    pubs = [pubkey(key) for key in keys]
    owners = [h("ACCOUNT", pub) for pub in pubs]
    peers = [h("PEER", pub) for pub in pubs]
    validators = [h("VALIDATOR", pub) for pub in pubs]
    chain = blob(b"test")
    lot_ids = [bytes([0x20 + i]) * 32 for i in range(5)]

    # Parameter fields follow the exact ADR 0008 order and width.
    parameters = b"".join((
        u64(1440), u64(60), u64(100), u32(262144), u32(1048576),
        u64(1048576), u64(524288), u64(1), u64(1), u64(100), u64(1000), u64(2),
        u16(3333), u64(2), u64(28 * 86400), u64(21 * 86400),
        u64(14 * 86400), u64(30), u16(500), u16(500), u64(28 * 86400),
    ))
    ordered_validators = sorted(zip(validators, owners, pubs))
    set_entries = [validator + owner + pub + u64(100) + u8(2)
                   for validator, owner, pub in ordered_validators]
    validator_set = items(set_entries, structured=True)
    set_root = h("VALSET", obj(10, validator_set))
    param_root = h("PARAMS", obj(11, parameters))
    initial_rule_set_hash = h(
        "RULESET", u16(1) + bytes([0x91]) * 32 + bytes([0x92]) * 32 + ZERO
    )

    lots = [lot_ids[i] + bytes([0x40 + i]) * 32 + owners[i] + u64(100) + u8(2)
            for i in range(4)]
    lots.append(lot_ids[4] + bytes([0x44]) * 32 + owners[0] + u64(1000) + u8(1))
    lots.sort(key=lambda raw: raw[:32])
    stakes = [bytes([0x60 + i]) * 32 + owners[i] + validators[i] + items([lot_ids[i]])
              for i in range(4)]
    stakes.sort(key=lambda raw: raw[:32])
    genesis = (chain + i64(1000) + initial_rule_set_hash
               + nested(parameters) + nested(validator_set)
               + items(sorted(pubs, key=lambda pub: h("ACCOUNT", pub)))
               + items(lots, structured=True) + items(stakes, structured=True)
               + items([u64(1) + u64(10) + u64(1)], structured=True))
    genesis_hash = h("GENESIS", obj(1, genesis))

    tx_unsigned = (chain + genesis_hash + u8(1) + pubs[0] + u64(1) + u64(3)
                   + u64(1) + u64(100) + items([lot_ids[4]]) + blob(owners[1]))
    transaction, tx_signature = signed(2, "SIGN/TX", tx_unsigned, keys[0])
    tx_bytes = obj(2, transaction)
    tx_id = h("TXID", tx_bytes)

    body = items([transaction], structured=True) + items([]) + items([])
    body_bytes = obj(4, body)
    state_root = bytes([0x77]) * 32
    receipt = tx_id + u64(100) + u64(100) + items([]) + state_root
    header = b"".join((
        chain, genesis_hash, u64(1), u16(1), initial_rule_set_hash,
        u16(1), initial_rule_set_hash, u64(1), genesis_hash,
        i64(1060), validators[0], set_root, set_root, param_root, u64(1), ZERO,
        h("BODY", body_bytes), merkle("tx", [tx_bytes]),
        merkle("receipt", [obj(5, receipt)]), merkle("evidence", []),
        state_root, u32(len(body_bytes)), u64(100),
    ))
    block_id = h("BLOCK", obj(3, header))

    def vote_payload(index: int, step: int, block: bytes) -> tuple[bytes, bytes]:
        unsigned = (chain + genesis_hash + u64(1) + u64(1) + u8(step)
                    + b"\x01" + block + validators[index] + i64(1061 + index))
        return signed(6, "SIGN/VOTE", unsigned, keys[index])

    votes = [vote_payload(i, 2, block_id)[0] for i in range(4)]
    votes.sort(key=lambda raw: raw[-104:-72])  # validator ID before i64+sig
    vote = vote_payload(0, 2, block_id)[0]
    qc = (u64(1) + u64(1) + u8(2) + b"\x01" + block_id + set_root
          + items(votes, structured=True) + u64(400) + u64(400) + u64(267))
    proposal_unsigned = nested(header) + nested(body) + b"\x00\x00"
    proposal, proposal_signature = signed(
        7, "SIGN/PROPOSAL", proposal_unsigned, keys[0]
    )
    alternate_block = h("BLOCK", b"conflict")
    bad_vote_a = vote_payload(0, 1, block_id)[0]
    bad_vote_b = vote_payload(0, 1, alternate_block)[0]
    signed_evidence = sorted((obj(6, bad_vote_a), obj(6, bad_vote_b)))
    evidence = u8(1) + blob(signed_evidence[0]) + blob(signed_evidence[1])
    artifact = (nested(header) + nested(body) + items([receipt], structured=True)
                + b"\x00" + nested(qc))
    leaf = u8(1) + blob(owners[0]) + blob(b"fixture")
    snapshot = (u64(1) + block_id + u16(1) + initial_rule_set_hash + state_root
                + items([leaf], structured=True) + items([qc], structured=True))
    envelope_unsigned = (chain + genesis_hash + u16(9) + peers[0]
                         + u64(1) + i64(1062) + u32(30) + blob(obj(6, vote))
                         + h("NET-PAYLOAD", obj(6, vote)))
    envelope, envelope_signature = signed(
        14, "SIGN/ENVELOPE", envelope_unsigned, keys[0]
    )

    # The AUTH signature has a dedicated transcript rather than a top-level kind.
    hello_payload = peers[0] + pubs[0] + bytes([0xa1]) * 32 + u32(0)
    hello_unsigned = (chain + genesis_hash + u16(1) + peers[0] + u64(1)
                      + i64(1062) + u32(30) + blob(hello_payload)
                      + h("NET-PAYLOAD", hello_payload))
    hello = obj(14, signed(14, "SIGN/ENVELOPE", hello_unsigned, keys[0])[0])
    challenge_payload = (peers[1] + pubs[1] + bytes([0xa2]) * 32
                         + h("HANDSHAKE-MSG", hello))
    challenge_unsigned = (chain + genesis_hash + u16(2) + peers[1] + u64(1)
                          + i64(1063) + u32(30) + blob(challenge_payload)
                          + h("NET-PAYLOAD", challenge_payload))
    challenge = obj(14, signed(14, "SIGN/ENVELOPE", challenge_unsigned,
                               keys[1])[0])
    fixture_traffic_keys = bytes([0xc1]) * 32 + bytes([0xc2]) * 32
    transcript = (b"NODO/V1/HANDSHAKE\0" + chain + genesis_hash
                  + blob(hello) + blob(challenge)
                  + h("HANDSHAKE-KEYS", fixture_traffic_keys) + u8(1))
    handshake_signature = keys[0].sign(h("SIGN/HANDSHAKE", transcript))
    peer_unsigned = (chain + genesis_hash + peers[0]
                     + blob(b"127.0.0.1:4242") + pubs[0] + i64(2000))
    peer_preimage = b"NODO/V1/PEER\0" + peer_unsigned
    peer_signature = keys[0].sign(h("SIGN/PEER", peer_preimage))

    payloads = (
        genesis, transaction, header, body, receipt, vote, proposal, qc,
        evidence, validator_set, parameters, artifact, snapshot, envelope,
    )
    vectors = {}
    for kind, (name, payload) in enumerate(zip(KINDS, payloads), 1):
        encoded = obj(kind, payload)
        vectors[name] = {
            "kind": kind,
            "size": len(encoded),
            "bytes_hex": encoded.hex(),
            "sha256_hex": hashlib.sha256(encoded).hexdigest(),
        }
    return {
        "format": "nodo-v1-encoding-fixtures-1",
        "note": "Signed fields have valid Ed25519 signatures; full chain-state validity is not asserted.",
        "signer_public_key_hex": pubs[0].hex(),
        "signatures_hex": {
            "transaction": tx_signature.hex(),
            "vote": vote[-64:].hex(),
            "proposal": proposal_signature.hex(),
            "network_envelope": envelope_signature.hex(),
        },
        "handshake_transcript": {
            "note": "Synthetic traffic keys test encoding only, not key exchange.",
            "responder_public_key_hex": pubs[1].hex(),
            "initiator_hello_hex": hello.hex(),
            "responder_challenge_hex": challenge.hex(),
            "traffic_keys_hex": fixture_traffic_keys.hex(),
            "preimage_hex": transcript.hex(),
            "signer_role": 1,
            "signature_hex": handshake_signature.hex(),
        },
        "signed_peer_record": {
            "preimage_hex": peer_preimage.hex(),
            "record_hex": (peer_unsigned + peer_signature).hex(),
            "signature_hex": peer_signature.hex(),
        },
        "vectors": vectors,
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true",
                        help="compare regenerated vectors with the committed file")
    args = parser.parse_args()
    result = make_vectors()
    signer = Ed25519PublicKey.from_public_bytes(
        bytes.fromhex(result["signer_public_key_hex"])
    )
    domains = {
        "transaction": "SIGN/TX",
        "vote": "SIGN/VOTE",
        "proposal": "SIGN/PROPOSAL",
        "network_envelope": "SIGN/ENVELOPE",
    }
    for name, vector in result["vectors"].items():
        encoded = bytes.fromhex(vector["bytes_hex"])
        assert len(encoded) == vector["size"]
        assert encoded[:8] == b"NODO" + u16(1) + u16(vector["kind"])
        assert hashlib.sha256(encoded).hexdigest() == vector["sha256_hex"]
        if name in domains:
            assert encoded[-64:].hex() == result["signatures_hex"][name]
            signer.verify(encoded[-64:], h(domains[name], encoded[:-64]))
    handshake = result["handshake_transcript"]
    signer.verify(bytes.fromhex(handshake["signature_hex"]),
                  h("SIGN/HANDSHAKE", bytes.fromhex(handshake["preimage_hex"])))
    hello = bytes.fromhex(handshake["initiator_hello_hex"])
    challenge = bytes.fromhex(handshake["responder_challenge_hex"])
    assert len(hello) <= 4096 and len(challenge) <= 4096
    signer.verify(hello[-64:], h("SIGN/ENVELOPE", hello[:-64]))
    responder = Ed25519PublicKey.from_public_bytes(
        bytes.fromhex(handshake["responder_public_key_hex"])
    )
    responder.verify(challenge[-64:], h("SIGN/ENVELOPE", challenge[:-64]))
    assert h("HANDSHAKE-MSG", hello) in challenge
    chain = blob(b"test")
    genesis_hash = h("GENESIS", bytes.fromhex(result["vectors"]["genesis"]["bytes_hex"]))
    assert bytes.fromhex(handshake["preimage_hex"]) == (
        b"NODO/V1/HANDSHAKE\0" + chain + genesis_hash + blob(hello)
        + blob(challenge)
        + h("HANDSHAKE-KEYS", bytes.fromhex(handshake["traffic_keys_hex"]))
        + u8(handshake["signer_role"])
    )
    peer_record = result["signed_peer_record"]
    peer_bytes = bytes.fromhex(peer_record["record_hex"])
    assert bytes.fromhex(peer_record["preimage_hex"]) == (
        b"NODO/V1/PEER\0" + peer_bytes[:-64]
    )
    assert peer_bytes[-64:].hex() == peer_record["signature_hex"]
    signer.verify(peer_bytes[-64:], h("SIGN/PEER", bytes.fromhex(
        peer_record["preimage_hex"])))
    content = json.dumps(result, indent=2) + "\n"
    if args.check:
        if OUTPUT.read_text(encoding="utf-8") != content:
            raise SystemExit("v1 object vectors differ; regenerate them")
        markdown = (ROOT / "docs" / "spec" / "vectors-v1.md").read_text(
            encoding="utf-8")
        for name, vector in result["vectors"].items():
            row = (f"| {vector['kind']} | {name.replace('_', ' ')} | "
                   f"{vector['size']} | `{vector['sha256_hex']}` |")
            if not re.search(r"^" + re.escape(row) + r"$", markdown,
                             flags=re.MULTILINE):
                raise SystemExit(f"missing or stale v1 vector table row: {name}")
        print("V1 object vectors match all 14 generated fixtures.")
    else:
        OUTPUT.write_text(content, encoding="utf-8")
        markdown_path = ROOT / "docs" / "spec" / "vectors-v1.md"
        markdown = markdown_path.read_text(encoding="utf-8")
        for name, vector in result["vectors"].items():
            row = (f"| {vector['kind']} | {name.replace('_', ' ')} | "
                   f"{vector['size']} | `{vector['sha256_hex']}` |")
            pattern = (r"^\| " + str(vector["kind"]) + r" \| "
                       + re.escape(name.replace("_", " "))
                       + r" \| \d+ \| `[0-9a-f]{64}` \|$")
            markdown, count = re.subn(pattern, lambda _: row, markdown,
                                      flags=re.MULTILINE)
            if count != 1:
                raise SystemExit(f"missing or duplicate vector row: {name}")
        markdown_path.write_text(markdown, encoding="utf-8")
        print(f"Wrote {OUTPUT}")


if __name__ == "__main__":
    main()
