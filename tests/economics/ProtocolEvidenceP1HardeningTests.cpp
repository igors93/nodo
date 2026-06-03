// P1 tests: ProtocolEvidence field limits, strict deserialize, duplicate/unknown field rejection.
#include "economics/ProtocolEvidence.hpp"

#include <cassert>
#include <stdexcept>
#include <string>

namespace {

using nodo::economics::ProtocolEvidence;
using nodo::economics::ProtocolEvidenceType;

ProtocolEvidence validEvidence() {
    return ProtocolEvidence(
        "evid-001",
        ProtocolEvidenceType::P2P_INVALID_MESSAGE,
        "peer-node-01",
        "local-node-01",
        100, 1,
        "p2p.inbound-validation",
        "abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890",
        "Peer sent malformed envelope.",
        1000000
    );
}

// Test 8: ProtocolEvidence rejects empty evidenceId.
void testRejectsEmptyEvidenceId() {
    const ProtocolEvidence e(
        "", ProtocolEvidenceType::P2P_INVALID_MESSAGE,
        "subject", "source", 0, 0, "rule", "digest", "reason", 0
    );
    assert(!e.isValid());
}

// Test 9: ProtocolEvidence rejects evidenceId exceeding 128 chars.
void testRejectsOversizedEvidenceId() {
    const std::string longId(129, 'a');
    const ProtocolEvidence e(
        longId, ProtocolEvidenceType::P2P_INVALID_MESSAGE,
        "subject", "source", 0, 0, "rule", "digest", "reason", 0
    );
    assert(!e.isValid());
}

// Test 10: ProtocolEvidence rejects empty payloadDigest.
void testRejectsEmptyPayloadDigest() {
    const ProtocolEvidence e(
        "evid-001", ProtocolEvidenceType::P2P_INVALID_MESSAGE,
        "subject", "source", 0, 0, "rule",
        "",  // empty payloadDigest
        "reason", 0
    );
    assert(!e.isValid());
}

// Test 11: ProtocolEvidence rejects reason exceeding 512 chars.
void testRejectsOversizedReason() {
    const std::string longReason(513, 'x');
    const ProtocolEvidence e(
        "evid-001", ProtocolEvidenceType::P2P_INVALID_MESSAGE,
        "subject", "source", 0, 0, "rule", "digest",
        longReason, 0
    );
    assert(!e.isValid());
}

// Test 12: Valid evidence serializes and deserializes correctly.
void testSerializeDeserializeRoundTrip() {
    const ProtocolEvidence original = validEvidence();
    assert(original.isValid());
    const std::string serialized = original.serialize();
    const ProtocolEvidence decoded = ProtocolEvidence::deserialize(serialized);
    assert(decoded.isValid());
    assert(decoded.evidenceId() == original.evidenceId());
    assert(decoded.subjectId() == original.subjectId());
    assert(decoded.sourceId() == original.sourceId());
    assert(decoded.ruleId() == original.ruleId());
    assert(decoded.payloadDigest() == original.payloadDigest());
    assert(decoded.reason() == original.reason());
}

// Test 13: Deserialize rejects unknown fields.
void testDeserializeRejectsUnknownField() {
    const std::string malformed =
        "ProtocolEvidence{evidenceId=x;unknownField=value}";
    bool threw = false;
    try {
        ProtocolEvidence::deserialize(malformed);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
}

// Test 14: Deserialize rejects duplicate fields.
void testDeserializeRejectsDuplicateField() {
    const std::string withDuplicate =
        "ProtocolEvidence{evidenceId=a;evidenceId=b}";
    bool threw = false;
    try {
        ProtocolEvidence::deserialize(withDuplicate);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
}

// Test 15: Deserialize rejects unknown evidenceType.
void testDeserializeRejectsUnknownEvidenceType() {
    const ProtocolEvidence original = validEvidence();
    std::string serialized = original.serialize();
    // Replace known type with garbage.
    const std::string oldType = "evidenceType=P2P_INVALID_MESSAGE";
    const std::string newType = "evidenceType=NOT_A_REAL_TYPE";
    const std::size_t pos = serialized.find(oldType);
    assert(pos != std::string::npos);
    serialized.replace(pos, oldType.size(), newType);
    bool threw = false;
    try {
        ProtocolEvidence::deserialize(serialized);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
}

} // namespace

int main() {
    testRejectsEmptyEvidenceId();
    testRejectsOversizedEvidenceId();
    testRejectsEmptyPayloadDigest();
    testRejectsOversizedReason();
    testSerializeDeserializeRoundTrip();
    testDeserializeRejectsUnknownField();
    testDeserializeRejectsDuplicateField();
    testDeserializeRejectsUnknownEvidenceType();
    return 0;
}
