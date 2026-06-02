#ifndef NODO_ECONOMICS_PROTOCOL_EVIDENCE_HPP
#define NODO_ECONOMICS_PROTOCOL_EVIDENCE_HPP

#include <cstdint>
#include <string>

namespace nodo::economics {

/*
 * ProtocolEvidenceType identifies the category of protocol violation or
 * observable event that this evidence records.
 */
enum class ProtocolEvidenceType {
    P2P_INVALID_MESSAGE,
    P2P_RATE_LIMIT_EXCEEDED,
    P2P_PEER_QUARANTINED,
    DATA_AVAILABILITY_FAILURE,
    DOUBLE_SIGN,
    INVALID_BLOCK_VOTE
};

std::string protocolEvidenceTypeToString(ProtocolEvidenceType type);

bool protocolEvidenceTypeFromString(
    const std::string& s,
    ProtocolEvidenceType& out
);

/*
 * ProtocolEvidence is a canonical, immutable record of a verifiable protocol
 * event. Evidence must be persisted atomically and may not be modified after
 * creation. The payloadDigest binds this record to the underlying event data.
 *
 * Security principle:
 * Evidence is observational only at P0. It does not automatically trigger
 * slashing or economic penalties. It provides a verifiable audit trail for
 * future slashing, protection scoring, and chain audit decisions.
 *
 * DoS protection:
 * Evidence records are rate-limited per subject/event type by the store layer.
 * payloadDigest must be non-empty to prevent evidence inflation.
 */
class ProtocolEvidence {
public:
    ProtocolEvidence();

    ProtocolEvidence(
        std::string evidenceId,
        ProtocolEvidenceType evidenceType,
        std::string subjectId,
        std::string sourceId,
        std::uint64_t blockHeight,
        std::uint64_t epoch,
        std::string ruleId,
        std::string payloadDigest,
        std::string reason,
        std::int64_t createdAt
    );

    const std::string& evidenceId() const;
    ProtocolEvidenceType evidenceType() const;
    const std::string& subjectId() const;
    const std::string& sourceId() const;
    std::uint64_t blockHeight() const;
    std::uint64_t epoch() const;
    const std::string& ruleId() const;
    const std::string& payloadDigest() const;
    const std::string& reason() const;
    std::int64_t createdAt() const;

    bool isValid() const;
    const std::string& rejectionReason() const;

    std::string serialize() const;
    static ProtocolEvidence deserialize(const std::string& serialized);

private:
    std::string m_evidenceId;
    ProtocolEvidenceType m_evidenceType;
    std::string m_subjectId;
    std::string m_sourceId;
    std::uint64_t m_blockHeight;
    std::uint64_t m_epoch;
    std::string m_ruleId;
    std::string m_payloadDigest;
    std::string m_reason;
    std::int64_t m_createdAt;
    bool m_valid;
    std::string m_rejectionReason;
};

} // namespace nodo::economics

#endif
