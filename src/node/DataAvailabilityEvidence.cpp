#include "node/DataAvailabilityEvidence.hpp"

#include <sstream>
#include <utility>

namespace nodo::node {

// --- DataAvailabilityChallenge ---

DataAvailabilityChallenge::DataAvailabilityChallenge()
    : m_blockHeight(0),
      m_issuedAt(0),
      m_valid(false),
      m_rejectionReason("DataAvailabilityChallenge: default-constructed.") {}

DataAvailabilityChallenge::DataAvailabilityChallenge(
    std::string challengeId,
    std::uint64_t blockHeight,
    std::string blockHash,
    std::string artifactDigest,
    std::string challengerId,
    std::int64_t issuedAt
)
    : m_challengeId(std::move(challengeId)),
      m_blockHeight(blockHeight),
      m_blockHash(std::move(blockHash)),
      m_artifactDigest(std::move(artifactDigest)),
      m_challengerId(std::move(challengerId)),
      m_issuedAt(issuedAt),
      m_valid(false),
      m_rejectionReason("")
{
    if (m_challengeId.empty()) {
        m_rejectionReason = "DataAvailabilityChallenge: challengeId must not be empty.";
        return;
    }
    if (m_blockHash.empty()) {
        m_rejectionReason = "DataAvailabilityChallenge: blockHash must not be empty.";
        return;
    }
    if (m_artifactDigest.empty()) {
        m_rejectionReason = "DataAvailabilityChallenge: artifactDigest must not be empty.";
        return;
    }
    if (m_challengerId.empty()) {
        m_rejectionReason = "DataAvailabilityChallenge: challengerId must not be empty.";
        return;
    }
    m_valid = true;
}

const std::string& DataAvailabilityChallenge::challengeId() const { return m_challengeId; }
std::uint64_t DataAvailabilityChallenge::blockHeight() const { return m_blockHeight; }
const std::string& DataAvailabilityChallenge::blockHash() const { return m_blockHash; }
const std::string& DataAvailabilityChallenge::artifactDigest() const { return m_artifactDigest; }
const std::string& DataAvailabilityChallenge::challengerId() const { return m_challengerId; }
std::int64_t DataAvailabilityChallenge::issuedAt() const { return m_issuedAt; }
bool DataAvailabilityChallenge::isValid() const { return m_valid; }
const std::string& DataAvailabilityChallenge::rejectionReason() const { return m_rejectionReason; }

std::string DataAvailabilityChallenge::serialize() const {
    std::ostringstream oss;
    oss << "DataAvailabilityChallenge{"
        << "challengeId=" << m_challengeId
        << ";blockHeight=" << m_blockHeight
        << ";blockHash=" << m_blockHash
        << ";artifactDigest=" << m_artifactDigest
        << ";challengerId=" << m_challengerId
        << ";issuedAt=" << m_issuedAt
        << "}";
    return oss.str();
}

// --- DataAvailabilityResponse ---

DataAvailabilityResponse::DataAvailabilityResponse()
    : m_respondedAt(0),
      m_valid(false),
      m_rejectionReason("DataAvailabilityResponse: default-constructed.") {}

DataAvailabilityResponse::DataAvailabilityResponse(
    std::string responseId,
    std::string challengeId,
    std::string serverId,
    std::string artifactDigest,
    std::int64_t respondedAt
)
    : m_responseId(std::move(responseId)),
      m_challengeId(std::move(challengeId)),
      m_serverId(std::move(serverId)),
      m_artifactDigest(std::move(artifactDigest)),
      m_respondedAt(respondedAt),
      m_valid(false),
      m_rejectionReason("")
{
    if (m_responseId.empty()) {
        m_rejectionReason = "DataAvailabilityResponse: responseId must not be empty.";
        return;
    }
    if (m_challengeId.empty()) {
        m_rejectionReason = "DataAvailabilityResponse: challengeId must not be empty.";
        return;
    }
    if (m_serverId.empty()) {
        m_rejectionReason = "DataAvailabilityResponse: serverId must not be empty.";
        return;
    }
    if (m_artifactDigest.empty()) {
        m_rejectionReason = "DataAvailabilityResponse: artifactDigest must not be empty.";
        return;
    }
    m_valid = true;
}

const std::string& DataAvailabilityResponse::responseId() const { return m_responseId; }
const std::string& DataAvailabilityResponse::challengeId() const { return m_challengeId; }
const std::string& DataAvailabilityResponse::serverId() const { return m_serverId; }
const std::string& DataAvailabilityResponse::artifactDigest() const { return m_artifactDigest; }
std::int64_t DataAvailabilityResponse::respondedAt() const { return m_respondedAt; }
bool DataAvailabilityResponse::isValid() const { return m_valid; }
const std::string& DataAvailabilityResponse::rejectionReason() const { return m_rejectionReason; }

std::string DataAvailabilityResponse::serialize() const {
    std::ostringstream oss;
    oss << "DataAvailabilityResponse{"
        << "responseId=" << m_responseId
        << ";challengeId=" << m_challengeId
        << ";serverId=" << m_serverId
        << ";artifactDigest=" << m_artifactDigest
        << ";respondedAt=" << m_respondedAt
        << "}";
    return oss.str();
}

// --- DataAvailabilityAttestation ---

DataAvailabilityAttestation::DataAvailabilityAttestation()
    : m_blockHeight(0),
      m_attestedAt(0),
      m_valid(false),
      m_rejectionReason("DataAvailabilityAttestation: default-constructed.") {}

DataAvailabilityAttestation::DataAvailabilityAttestation(
    std::string attestationId,
    std::uint64_t blockHeight,
    std::string artifactDigest,
    std::string attestorId,
    std::int64_t attestedAt
)
    : m_attestationId(std::move(attestationId)),
      m_blockHeight(blockHeight),
      m_artifactDigest(std::move(artifactDigest)),
      m_attestorId(std::move(attestorId)),
      m_attestedAt(attestedAt),
      m_valid(false),
      m_rejectionReason("")
{
    if (m_attestationId.empty()) {
        m_rejectionReason = "DataAvailabilityAttestation: attestationId must not be empty.";
        return;
    }
    if (m_artifactDigest.empty()) {
        m_rejectionReason = "DataAvailabilityAttestation: artifactDigest must not be empty.";
        return;
    }
    if (m_attestorId.empty()) {
        m_rejectionReason = "DataAvailabilityAttestation: attestorId must not be empty.";
        return;
    }
    m_valid = true;
}

const std::string& DataAvailabilityAttestation::attestationId() const { return m_attestationId; }
std::uint64_t DataAvailabilityAttestation::blockHeight() const { return m_blockHeight; }
const std::string& DataAvailabilityAttestation::artifactDigest() const { return m_artifactDigest; }
const std::string& DataAvailabilityAttestation::attestorId() const { return m_attestorId; }
std::int64_t DataAvailabilityAttestation::attestedAt() const { return m_attestedAt; }
bool DataAvailabilityAttestation::isValid() const { return m_valid; }
const std::string& DataAvailabilityAttestation::rejectionReason() const { return m_rejectionReason; }

std::string DataAvailabilityAttestation::serialize() const {
    std::ostringstream oss;
    oss << "DataAvailabilityAttestation{"
        << "attestationId=" << m_attestationId
        << ";blockHeight=" << m_blockHeight
        << ";artifactDigest=" << m_artifactDigest
        << ";attestorId=" << m_attestorId
        << ";attestedAt=" << m_attestedAt
        << "}";
    return oss.str();
}

// --- DataAvailabilityFailureEvidence ---

DataAvailabilityFailureEvidence::DataAvailabilityFailureEvidence()
    : m_blockHeight(0),
      m_recordedAt(0),
      m_valid(false),
      m_rejectionReason("DataAvailabilityFailureEvidence: default-constructed.") {}

DataAvailabilityFailureEvidence::DataAvailabilityFailureEvidence(
    std::string evidenceId,
    std::string challengeId,
    std::uint64_t blockHeight,
    std::string failedNodeId,
    std::string reason,
    std::int64_t recordedAt
)
    : m_evidenceId(std::move(evidenceId)),
      m_challengeId(std::move(challengeId)),
      m_blockHeight(blockHeight),
      m_failedNodeId(std::move(failedNodeId)),
      m_reason(std::move(reason)),
      m_recordedAt(recordedAt),
      m_valid(false),
      m_rejectionReason("")
{
    if (m_evidenceId.empty()) {
        m_rejectionReason = "DataAvailabilityFailureEvidence: evidenceId must not be empty.";
        return;
    }
    if (m_challengeId.empty()) {
        m_rejectionReason = "DataAvailabilityFailureEvidence: challengeId must not be empty.";
        return;
    }
    if (m_failedNodeId.empty()) {
        m_rejectionReason = "DataAvailabilityFailureEvidence: failedNodeId must not be empty.";
        return;
    }
    if (m_reason.empty()) {
        m_rejectionReason = "DataAvailabilityFailureEvidence: reason must not be empty.";
        return;
    }
    m_valid = true;
}

const std::string& DataAvailabilityFailureEvidence::evidenceId() const { return m_evidenceId; }
const std::string& DataAvailabilityFailureEvidence::challengeId() const { return m_challengeId; }
std::uint64_t DataAvailabilityFailureEvidence::blockHeight() const { return m_blockHeight; }
const std::string& DataAvailabilityFailureEvidence::failedNodeId() const { return m_failedNodeId; }
const std::string& DataAvailabilityFailureEvidence::reason() const { return m_reason; }
std::int64_t DataAvailabilityFailureEvidence::recordedAt() const { return m_recordedAt; }
bool DataAvailabilityFailureEvidence::isValid() const { return m_valid; }
const std::string& DataAvailabilityFailureEvidence::rejectionReason() const { return m_rejectionReason; }

std::string DataAvailabilityFailureEvidence::serialize() const {
    std::ostringstream oss;
    oss << "DataAvailabilityFailureEvidence{"
        << "evidenceId=" << m_evidenceId
        << ";challengeId=" << m_challengeId
        << ";blockHeight=" << m_blockHeight
        << ";failedNodeId=" << m_failedNodeId
        << ";reason=" << m_reason
        << ";recordedAt=" << m_recordedAt
        << "}";
    return oss.str();
}

} // namespace nodo::node
