#include "p2p/PeerAbuseEvidence.hpp"

#include <cassert>
#include <string>

namespace {

using nodo::p2p::PeerAbuseAction;
using nodo::p2p::PeerAbuseEvidence;
using nodo::p2p::peerAbuseActionToString;

PeerAbuseEvidence validEvidence(
    const std::string& evidenceId = "pae-001",
    PeerAbuseAction action = PeerAbuseAction::QUARANTINED
) {
    return PeerAbuseEvidence(
        evidenceId,
        "peer-xyz",
        "VOTE",
        "sha256:deadbeef",
        "local-observer",
        100,
        action,
        "Peer sent malformed vote message.",
        1900000000
    );
}

void testValidEvidenceAccepted() {
    const auto ev = validEvidence();
    assert(ev.isValid());
    assert(ev.evidenceId() == "pae-001");
    assert(ev.peerId() == "peer-xyz");
    assert(ev.action() == PeerAbuseAction::QUARANTINED);
}

void testEmptyEvidenceIdRejected() {
    const auto ev = validEvidence("");
    assert(!ev.isValid());
}

void testEmptyPeerIdRejected() {
    const PeerAbuseEvidence ev("id", "", "type", "digest", "observer", 1,
                                PeerAbuseAction::REJECTED, "reason", 0);
    assert(!ev.isValid());
}

void testEmptyPayloadDigestRejected() {
    const PeerAbuseEvidence ev("id", "peer", "type", "", "observer", 1,
                                PeerAbuseAction::REJECTED, "reason", 0);
    assert(!ev.isValid());
}

void testEmptyReasonRejected() {
    const PeerAbuseEvidence ev("id", "peer", "type", "digest", "observer", 1,
                                PeerAbuseAction::REJECTED, "", 0);
    assert(!ev.isValid());
}

void testActionToString() {
    assert(peerAbuseActionToString(PeerAbuseAction::REJECTED) == "REJECTED");
    assert(peerAbuseActionToString(PeerAbuseAction::RATE_LIMITED) == "RATE_LIMITED");
    assert(peerAbuseActionToString(PeerAbuseAction::QUARANTINED) == "QUARANTINED");
    assert(peerAbuseActionToString(PeerAbuseAction::BANNED) == "BANNED");
}

void testConvertsToProtocolEvidence() {
    const auto pae = validEvidence("pae-convert", PeerAbuseAction::QUARANTINED);
    assert(pae.isValid());

    const auto pe = pae.toProtocolEvidence();
    assert(pe.isValid());
    assert(pe.subjectId() == pae.peerId());
    assert(pe.evidenceType() == nodo::economics::ProtocolEvidenceType::P2P_PEER_QUARANTINED);
    assert(pe.payloadDigest() == pae.payloadDigest());
}

void testRateLimitedConvertsCorrectType() {
    const auto pae = validEvidence("pae-rl", PeerAbuseAction::RATE_LIMITED);
    assert(pae.isValid());
    const auto pe = pae.toProtocolEvidence();
    assert(pe.evidenceType() == nodo::economics::ProtocolEvidenceType::P2P_RATE_LIMIT_EXCEEDED);
}

void testBannedConvertsCorrectType() {
    const auto pae = validEvidence("pae-ban", PeerAbuseAction::BANNED);
    assert(pae.isValid());
    const auto pe = pae.toProtocolEvidence();
    assert(pe.evidenceType() == nodo::economics::ProtocolEvidenceType::P2P_PEER_BANNED);
}

void testSerialize() {
    const auto ev = validEvidence();
    assert(ev.isValid());
    assert(!ev.serialize().empty());
}

} // namespace

int main() {
    testValidEvidenceAccepted();
    testEmptyEvidenceIdRejected();
    testEmptyPeerIdRejected();
    testEmptyPayloadDigestRejected();
    testEmptyReasonRejected();
    testActionToString();
    testConvertsToProtocolEvidence();
    testRateLimitedConvertsCorrectType();
    testBannedConvertsCorrectType();
    testSerialize();
    return 0;
}
