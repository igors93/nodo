#include "node/ValidatorPenaltyMessages.hpp"

#include <sstream>
#include <utility>

namespace nodo::node {

namespace {

bool isSafeScalar(const std::string& value, std::size_t maxSize = 240) {
    if (value.empty() || value.size() > maxSize) {
        return false;
    }

    for (const char character : value) {
        const bool allowed =
            (character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') ||
            character == '_' || character == '-' || character == '.' ||
            character == ':' || character == '/';
        if (!allowed) {
            return false;
        }
    }

    return true;
}

} // namespace

ValidatorPenaltyAnnouncement::ValidatorPenaltyAnnouncement()
    : m_networkId(""),
      m_chainId(""),
      m_announcerNodeId(""),
      m_decision(),
      m_announcedAt(0) {}

ValidatorPenaltyAnnouncement::ValidatorPenaltyAnnouncement(
    std::string networkId,
    std::string chainId,
    std::string announcerNodeId,
    consensus::ValidatorPenaltyDecision decision,
    std::int64_t announcedAt
) : m_networkId(std::move(networkId)),
    m_chainId(std::move(chainId)),
    m_announcerNodeId(std::move(announcerNodeId)),
    m_decision(std::move(decision)),
    m_announcedAt(announcedAt) {}

const std::string& ValidatorPenaltyAnnouncement::networkId() const { return m_networkId; }
const std::string& ValidatorPenaltyAnnouncement::chainId() const { return m_chainId; }
const std::string& ValidatorPenaltyAnnouncement::announcerNodeId() const { return m_announcerNodeId; }
const consensus::ValidatorPenaltyDecision& ValidatorPenaltyAnnouncement::decision() const { return m_decision; }
std::int64_t ValidatorPenaltyAnnouncement::announcedAt() const { return m_announcedAt; }

bool ValidatorPenaltyAnnouncement::isValid() const {
    return isSafeScalar(m_networkId) &&
           isSafeScalar(m_chainId) &&
           isSafeScalar(m_announcerNodeId) &&
           m_decision.isValid() &&
           m_announcedAt >= m_decision.createdAt();
}

std::string ValidatorPenaltyAnnouncement::serialize() const {
    std::ostringstream output;
    output << "ValidatorPenaltyAnnouncement{"
           << "networkId=" << m_networkId
           << ";chainId=" << m_chainId
           << ";announcerNodeId=" << m_announcerNodeId
           << ";decision=" << m_decision.serialize()
           << ";announcedAt=" << m_announcedAt
           << "}";
    return output.str();
}

ValidatorPenaltyRequest::ValidatorPenaltyRequest()
    : m_requesterNodeId(""),
      m_penaltyId(""),
      m_requestedAt(0) {}

ValidatorPenaltyRequest::ValidatorPenaltyRequest(
    std::string requesterNodeId,
    std::string penaltyId,
    std::int64_t requestedAt
) : m_requesterNodeId(std::move(requesterNodeId)),
    m_penaltyId(std::move(penaltyId)),
    m_requestedAt(requestedAt) {}

const std::string& ValidatorPenaltyRequest::requesterNodeId() const { return m_requesterNodeId; }
const std::string& ValidatorPenaltyRequest::penaltyId() const { return m_penaltyId; }
std::int64_t ValidatorPenaltyRequest::requestedAt() const { return m_requestedAt; }

bool ValidatorPenaltyRequest::isValid() const {
    return isSafeScalar(m_requesterNodeId) &&
           isSafeScalar(m_penaltyId) &&
           m_requestedAt > 0;
}

std::string ValidatorPenaltyRequest::serialize() const {
    std::ostringstream output;
    output << "ValidatorPenaltyRequest{"
           << "requesterNodeId=" << m_requesterNodeId
           << ";penaltyId=" << m_penaltyId
           << ";requestedAt=" << m_requestedAt
           << "}";
    return output.str();
}

} // namespace nodo::node
