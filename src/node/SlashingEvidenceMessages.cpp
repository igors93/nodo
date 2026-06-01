#include "node/SlashingEvidenceMessages.hpp"

#include <sstream>
#include <utility>

namespace nodo::node {

namespace {

bool isSafeScalar(const std::string& value, std::size_t maxSize = 200) {
    if (value.empty() || value.size() > maxSize) {
        return false;
    }

    for (const char character : value) {
        const bool allowed =
            (character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') ||
            character == '_' ||
            character == '-' ||
            character == '.' ||
            character == ':' ||
            character == '/';

        if (!allowed) {
            return false;
        }
    }

    return true;
}

} // namespace

SlashingEvidenceAnnouncement::SlashingEvidenceAnnouncement()
    : m_networkId(),
      m_chainId(),
      m_announcerNodeId(),
      m_record(),
      m_announcedAt(0) {}

SlashingEvidenceAnnouncement::SlashingEvidenceAnnouncement(
    std::string networkId,
    std::string chainId,
    std::string announcerNodeId,
    consensus::SlashingEvidenceRecord record,
    std::int64_t announcedAt
) : m_networkId(std::move(networkId)),
    m_chainId(std::move(chainId)),
    m_announcerNodeId(std::move(announcerNodeId)),
    m_record(std::move(record)),
    m_announcedAt(announcedAt) {}

const std::string& SlashingEvidenceAnnouncement::networkId() const {
    return m_networkId;
}

const std::string& SlashingEvidenceAnnouncement::chainId() const {
    return m_chainId;
}

const std::string& SlashingEvidenceAnnouncement::announcerNodeId() const {
    return m_announcerNodeId;
}

const consensus::SlashingEvidenceRecord& SlashingEvidenceAnnouncement::record() const {
    return m_record;
}

std::int64_t SlashingEvidenceAnnouncement::announcedAt() const {
    return m_announcedAt;
}

bool SlashingEvidenceAnnouncement::isValid() const {
    return isSafeScalar(m_networkId) &&
           isSafeScalar(m_chainId) &&
           isSafeScalar(m_announcerNodeId) &&
           m_record.isValid() &&
           m_announcedAt > 0;
}

std::string SlashingEvidenceAnnouncement::serialize() const {
    std::ostringstream output;
    output << "SlashingEvidenceAnnouncement{"
           << "networkId=" << m_networkId
           << ";chainId=" << m_chainId
           << ";announcerNodeId=" << m_announcerNodeId
           << ";announcedAt=" << m_announcedAt
           << ";record=" << m_record.serialize()
           << "}";
    return output.str();
}

SlashingEvidenceRequest::SlashingEvidenceRequest()
    : m_requesterNodeId(),
      m_evidenceId(),
      m_requestedAt(0) {}

SlashingEvidenceRequest::SlashingEvidenceRequest(
    std::string requesterNodeId,
    std::string evidenceId,
    std::int64_t requestedAt
) : m_requesterNodeId(std::move(requesterNodeId)),
    m_evidenceId(std::move(evidenceId)),
    m_requestedAt(requestedAt) {}

const std::string& SlashingEvidenceRequest::requesterNodeId() const {
    return m_requesterNodeId;
}

const std::string& SlashingEvidenceRequest::evidenceId() const {
    return m_evidenceId;
}

std::int64_t SlashingEvidenceRequest::requestedAt() const {
    return m_requestedAt;
}

bool SlashingEvidenceRequest::isValid() const {
    return isSafeScalar(m_requesterNodeId) &&
           isSafeScalar(m_evidenceId) &&
           m_requestedAt > 0;
}

std::string SlashingEvidenceRequest::serialize() const {
    std::ostringstream output;
    output << "SlashingEvidenceRequest{"
           << "requesterNodeId=" << m_requesterNodeId
           << ";evidenceId=" << m_evidenceId
           << ";requestedAt=" << m_requestedAt
           << "}";
    return output.str();
}

} // namespace nodo::node
