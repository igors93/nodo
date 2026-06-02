#include "p2p/PeerAbuseEvidence.hpp"

#include <sstream>
#include <utility>

namespace nodo::p2p {

std::string peerAbuseActionToString(PeerAbuseAction action) {
    switch (action) {
        case PeerAbuseAction::REJECTED:     return "REJECTED";
        case PeerAbuseAction::RATE_LIMITED: return "RATE_LIMITED";
        case PeerAbuseAction::QUARANTINED:  return "QUARANTINED";
        default:                             return "UNKNOWN";
    }
}

PeerAbuseEvidence::PeerAbuseEvidence()
    : m_blockHeight(0),
      m_action(PeerAbuseAction::REJECTED),
      m_observedAt(0),
      m_valid(false),
      m_rejectionReason("PeerAbuseEvidence: default-constructed.") {}

PeerAbuseEvidence::PeerAbuseEvidence(
    std::string evidenceId,
    std::string peerId,
    std::string messageType,
    std::string payloadDigest,
    std::string observerNodeId,
    std::uint64_t blockHeight,
    PeerAbuseAction action,
    std::string reason,
    std::int64_t observedAt
)
    : m_evidenceId(std::move(evidenceId)),
      m_peerId(std::move(peerId)),
      m_messageType(std::move(messageType)),
      m_payloadDigest(std::move(payloadDigest)),
      m_observerNodeId(std::move(observerNodeId)),
      m_blockHeight(blockHeight),
      m_action(action),
      m_reason(std::move(reason)),
      m_observedAt(observedAt),
      m_valid(false),
      m_rejectionReason("")
{
    if (m_evidenceId.empty()) {
        m_rejectionReason = "PeerAbuseEvidence: evidenceId must not be empty.";
        return;
    }
    if (m_peerId.empty()) {
        m_rejectionReason = "PeerAbuseEvidence: peerId must not be empty.";
        return;
    }
    if (m_payloadDigest.empty()) {
        m_rejectionReason = "PeerAbuseEvidence: payloadDigest must not be empty.";
        return;
    }
    if (m_observerNodeId.empty()) {
        m_rejectionReason = "PeerAbuseEvidence: observerNodeId must not be empty.";
        return;
    }
    if (m_reason.empty()) {
        m_rejectionReason = "PeerAbuseEvidence: reason must not be empty.";
        return;
    }
    m_valid = true;
}

const std::string& PeerAbuseEvidence::evidenceId() const { return m_evidenceId; }
const std::string& PeerAbuseEvidence::peerId() const { return m_peerId; }
const std::string& PeerAbuseEvidence::messageType() const { return m_messageType; }
const std::string& PeerAbuseEvidence::payloadDigest() const { return m_payloadDigest; }
const std::string& PeerAbuseEvidence::observerNodeId() const { return m_observerNodeId; }
std::uint64_t PeerAbuseEvidence::blockHeight() const { return m_blockHeight; }
PeerAbuseAction PeerAbuseEvidence::action() const { return m_action; }
const std::string& PeerAbuseEvidence::reason() const { return m_reason; }
std::int64_t PeerAbuseEvidence::observedAt() const { return m_observedAt; }
bool PeerAbuseEvidence::isValid() const { return m_valid; }
const std::string& PeerAbuseEvidence::rejectionReason() const { return m_rejectionReason; }

economics::ProtocolEvidence PeerAbuseEvidence::toProtocolEvidence() const {
    economics::ProtocolEvidenceType evidenceType;
    switch (m_action) {
        case PeerAbuseAction::QUARANTINED:
            evidenceType = economics::ProtocolEvidenceType::P2P_PEER_QUARANTINED;
            break;
        case PeerAbuseAction::RATE_LIMITED:
            evidenceType = economics::ProtocolEvidenceType::P2P_RATE_LIMIT_EXCEEDED;
            break;
        default:
            evidenceType = economics::ProtocolEvidenceType::P2P_INVALID_MESSAGE;
            break;
    }

    return economics::ProtocolEvidence(
        m_evidenceId,
        evidenceType,
        m_peerId,
        m_observerNodeId,
        m_blockHeight,
        0,
        "p2p." + peerAbuseActionToString(m_action),
        m_payloadDigest,
        m_reason,
        m_observedAt
    );
}

std::string PeerAbuseEvidence::serialize() const {
    std::ostringstream oss;
    oss << "PeerAbuseEvidence{"
        << "evidenceId=" << m_evidenceId
        << ";peerId=" << m_peerId
        << ";messageType=" << m_messageType
        << ";payloadDigest=" << m_payloadDigest
        << ";observerNodeId=" << m_observerNodeId
        << ";blockHeight=" << m_blockHeight
        << ";action=" << peerAbuseActionToString(m_action)
        << ";reason=" << m_reason
        << ";observedAt=" << m_observedAt
        << "}";
    return oss.str();
}

} // namespace nodo::p2p
