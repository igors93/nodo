#include "economics/ProtocolEvidence.hpp"

#include <cassert>
#include <string>

namespace {

using nodo::economics::ProtocolEvidence;
using nodo::economics::ProtocolEvidenceType;
using nodo::economics::protocolEvidenceTypeToString;
using nodo::economics::protocolEvidenceTypeFromString;

ProtocolEvidence validEvidence(
    const std::string& evidenceId = "ev-001",
    ProtocolEvidenceType type = ProtocolEvidenceType::P2P_INVALID_MESSAGE
) {
    return ProtocolEvidence(
        evidenceId,
        type,
        "peer-abc",
        "local-node",
        100,
        1,
        "p2p.invalid_message",
        "sha256:abc123",
        "Message failed validation.",
        1900000000
    );
}

void testValidEvidenceAccepted() {
    const auto ev = validEvidence();
    assert(ev.isValid());
    assert(ev.evidenceId() == "ev-001");
    assert(ev.subjectId() == "peer-abc");
    assert(ev.blockHeight() == 100);
}

void testEmptyEvidenceIdRejected() {
    const ProtocolEvidence ev(
        "", ProtocolEvidenceType::P2P_INVALID_MESSAGE,
        "peer", "local", 1, 0, "rule", "digest", "reason", 0
    );
    assert(!ev.isValid());
}

void testEmptySubjectIdRejected() {
    const ProtocolEvidence ev(
        "ev-001", ProtocolEvidenceType::P2P_INVALID_MESSAGE,
        "", "local", 1, 0, "rule", "digest", "reason", 0
    );
    assert(!ev.isValid());
}

void testEmptyPayloadDigestRejected() {
    const ProtocolEvidence ev(
        "ev-001", ProtocolEvidenceType::P2P_INVALID_MESSAGE,
        "peer", "local", 1, 0, "rule", "", "reason", 0
    );
    assert(!ev.isValid());
}

void testTypeRoundTrip() {
    const ProtocolEvidenceType types[] = {
        ProtocolEvidenceType::P2P_INVALID_MESSAGE,
        ProtocolEvidenceType::P2P_RATE_LIMIT_EXCEEDED,
        ProtocolEvidenceType::P2P_PEER_QUARANTINED,
        ProtocolEvidenceType::DATA_AVAILABILITY_FAILURE,
        ProtocolEvidenceType::DOUBLE_SIGN,
        ProtocolEvidenceType::INVALID_BLOCK_VOTE
    };
    for (const auto& t : types) {
        ProtocolEvidenceType parsed = ProtocolEvidenceType::P2P_INVALID_MESSAGE;
        assert(protocolEvidenceTypeFromString(protocolEvidenceTypeToString(t), parsed));
        assert(parsed == t);
    }
}

void testUnknownTypeRejected() {
    ProtocolEvidenceType out = ProtocolEvidenceType::P2P_INVALID_MESSAGE;
    assert(!protocolEvidenceTypeFromString("UNKNOWN", out));
    assert(!protocolEvidenceTypeFromString("", out));
}

void testSerializeDeserializeRoundTrip() {
    const auto original = validEvidence("ev-round-trip", ProtocolEvidenceType::P2P_PEER_QUARANTINED);
    assert(original.isValid());

    const std::string serialized = original.serialize();
    assert(!serialized.empty());

    const ProtocolEvidence deserialized = ProtocolEvidence::deserialize(serialized);
    assert(deserialized.isValid());
    assert(deserialized.evidenceId() == original.evidenceId());
    assert(deserialized.evidenceType() == original.evidenceType());
    assert(deserialized.subjectId() == original.subjectId());
    assert(deserialized.payloadDigest() == original.payloadDigest());
    assert(deserialized.blockHeight() == original.blockHeight());
}

void testDeserializeInvalidInputThrows() {
    bool threw = false;
    try {
        ProtocolEvidence::deserialize("not-valid-format");
    } catch (...) {
        threw = true;
    }
    assert(threw);
}

} // namespace

int main() {
    testValidEvidenceAccepted();
    testEmptyEvidenceIdRejected();
    testEmptySubjectIdRejected();
    testEmptyPayloadDigestRejected();
    testTypeRoundTrip();
    testUnknownTypeRejected();
    testSerializeDeserializeRoundTrip();
    testDeserializeInvalidInputThrows();
    return 0;
}
