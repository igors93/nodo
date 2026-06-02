#ifndef NODO_NODE_DATA_AVAILABILITY_EVIDENCE_HPP
#define NODO_NODE_DATA_AVAILABILITY_EVIDENCE_HPP

#include <cstdint>
#include <string>

namespace nodo::node {

/*
 * DataAvailabilityChallenge represents a verifiable request to prove that a
 * node holds a specific finalized artifact. The artifactDigest binds the
 * challenge to an exact artifact state so that no ambiguous response is
 * accepted.
 */
class DataAvailabilityChallenge {
public:
    DataAvailabilityChallenge();

    DataAvailabilityChallenge(
        std::string challengeId,
        std::uint64_t blockHeight,
        std::string blockHash,
        std::string artifactDigest,
        std::string challengerId,
        std::int64_t issuedAt
    );

    const std::string& challengeId() const;
    std::uint64_t blockHeight() const;
    const std::string& blockHash() const;
    const std::string& artifactDigest() const;
    const std::string& challengerId() const;
    std::int64_t issuedAt() const;

    bool isValid() const;
    const std::string& rejectionReason() const;

    std::string serialize() const;

private:
    std::string m_challengeId;
    std::uint64_t m_blockHeight;
    std::string m_blockHash;
    std::string m_artifactDigest;
    std::string m_challengerId;
    std::int64_t m_issuedAt;
    bool m_valid;
    std::string m_rejectionReason;
};

/*
 * DataAvailabilityResponse proves that a node possesses and can serve the
 * exact artifact referenced by a challenge. The artifactDigest must match
 * the challenge's artifactDigest.
 */
class DataAvailabilityResponse {
public:
    DataAvailabilityResponse();

    DataAvailabilityResponse(
        std::string responseId,
        std::string challengeId,
        std::string serverId,
        std::string artifactDigest,
        std::int64_t respondedAt
    );

    const std::string& responseId() const;
    const std::string& challengeId() const;
    const std::string& serverId() const;
    const std::string& artifactDigest() const;
    std::int64_t respondedAt() const;

    bool isValid() const;
    const std::string& rejectionReason() const;

    std::string serialize() const;

private:
    std::string m_responseId;
    std::string m_challengeId;
    std::string m_serverId;
    std::string m_artifactDigest;
    std::int64_t m_respondedAt;
    bool m_valid;
    std::string m_rejectionReason;
};

/*
 * DataAvailabilityAttestation records a node's assertion that a specific
 * artifact is locally held and can be served. Attestations without a
 * corresponding challenge are still useful for audit trails.
 */
class DataAvailabilityAttestation {
public:
    DataAvailabilityAttestation();

    DataAvailabilityAttestation(
        std::string attestationId,
        std::uint64_t blockHeight,
        std::string artifactDigest,
        std::string attestorId,
        std::int64_t attestedAt
    );

    const std::string& attestationId() const;
    std::uint64_t blockHeight() const;
    const std::string& artifactDigest() const;
    const std::string& attestorId() const;
    std::int64_t attestedAt() const;

    bool isValid() const;
    const std::string& rejectionReason() const;

    std::string serialize() const;

private:
    std::string m_attestationId;
    std::uint64_t m_blockHeight;
    std::string m_artifactDigest;
    std::string m_attestorId;
    std::int64_t m_attestedAt;
    bool m_valid;
    std::string m_rejectionReason;
};

/*
 * DataAvailabilityFailureEvidence records a verifiable failure: a challenge
 * was issued and the challenged node either did not respond or responded with
 * an artifact that does not match the expected digest.
 *
 * Security principle:
 * Failure evidence may only be created when both the challenge and the
 * observed failure (no response or invalid response) are verifiable. It does
 * not automatically trigger slashing at P0.
 */
class DataAvailabilityFailureEvidence {
public:
    DataAvailabilityFailureEvidence();

    DataAvailabilityFailureEvidence(
        std::string evidenceId,
        std::string challengeId,
        std::uint64_t blockHeight,
        std::string failedNodeId,
        std::string reason,
        std::int64_t recordedAt
    );

    const std::string& evidenceId() const;
    const std::string& challengeId() const;
    std::uint64_t blockHeight() const;
    const std::string& failedNodeId() const;
    const std::string& reason() const;
    std::int64_t recordedAt() const;

    bool isValid() const;
    const std::string& rejectionReason() const;

    std::string serialize() const;

private:
    std::string m_evidenceId;
    std::string m_challengeId;
    std::uint64_t m_blockHeight;
    std::string m_failedNodeId;
    std::string m_reason;
    std::int64_t m_recordedAt;
    bool m_valid;
    std::string m_rejectionReason;
};

} // namespace nodo::node

#endif
