#ifndef NODO_CONSENSUS_SLASHING_EVIDENCE_HPP
#define NODO_CONSENSUS_SLASHING_EVIDENCE_HPP

#include "consensus/ValidatorVoteRecord.hpp"
#include "core/ValidatorRegistry.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/SignatureProvider.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::consensus {

enum class SlashingEvidenceType {
    UNKNOWN,
    DOUBLE_VOTE,
    INVALID_SIGNATURE,
    EQUIVOCATION
};

std::string slashingEvidenceTypeToString(SlashingEvidenceType type);
SlashingEvidenceType slashingEvidenceTypeFromString(const std::string& value);

enum class SlashingEvidenceSeverity {
    UNKNOWN,
    WARNING,
    SLASHABLE
};

std::string slashingEvidenceSeverityToString(SlashingEvidenceSeverity severity);
SlashingEvidenceSeverity slashingEvidenceSeverityFromString(const std::string& value);

enum class SlashingEvidenceValidationStatus {
    ACCEPTED,
    DUPLICATE,
    REJECTED
};

std::string slashingEvidenceValidationStatusToString(
    SlashingEvidenceValidationStatus status
);

class SlashingEvidenceRecord {
public:
    SlashingEvidenceRecord();

    SlashingEvidenceRecord(
        std::string evidenceId,
        SlashingEvidenceType type,
        std::string validatorAddress,
        std::string payloadHash,
        SlashingEvidenceSeverity severity,
        std::int64_t createdAt
    );

    const std::string& evidenceId() const;
    SlashingEvidenceType type() const;
    const std::string& validatorAddress() const;
    const std::string& payloadHash() const;
    SlashingEvidenceSeverity severity() const;
    std::int64_t createdAt() const;

    bool isValid() const;
    std::string serialize() const;

    static SlashingEvidenceRecord deserialize(const std::string& serialized);

private:
    std::string m_evidenceId;
    SlashingEvidenceType m_type;
    std::string m_validatorAddress;
    std::string m_payloadHash;
    SlashingEvidenceSeverity m_severity;
    std::int64_t m_createdAt;
};

class DoubleVoteEvidence {
public:
    DoubleVoteEvidence();

    DoubleVoteEvidence(
        ValidatorVoteRecord firstVote,
        ValidatorVoteRecord secondVote,
        std::int64_t detectedAt
    );

    const ValidatorVoteRecord& firstVote() const;
    const ValidatorVoteRecord& secondVote() const;
    std::int64_t detectedAt() const;

    bool isConflictPair() const;
    std::string validatorAddress() const;
    std::string payload() const;
    std::string payloadHash() const;
    std::string evidenceId() const;

    SlashingEvidenceRecord toRecord() const;
    std::string serialize() const;

    static DoubleVoteEvidence deserialize(const std::string& serialized);

private:
    ValidatorVoteRecord m_firstVote;
    ValidatorVoteRecord m_secondVote;
    std::int64_t m_detectedAt;
};

class ProposerEquivocationEvidence {
public:
    ProposerEquivocationEvidence();

    ProposerEquivocationEvidence(
        std::string firstProposal,
        std::string secondProposal,
        std::string proposerAddress,
        std::uint64_t blockIndex,
        std::uint64_t round,
        std::string firstBlockHash,
        std::string secondBlockHash,
        std::int64_t detectedAt
    );

    const std::string& firstProposal() const;
    const std::string& secondProposal() const;
    const std::string& proposerAddress() const;
    std::uint64_t blockIndex() const;
    std::uint64_t round() const;
    const std::string& firstBlockHash() const;
    const std::string& secondBlockHash() const;
    std::int64_t detectedAt() const;

    bool isConflictPair() const;
    std::string payload() const;
    std::string payloadHash() const;
    std::string evidenceId() const;

    SlashingEvidenceRecord toRecord() const;
    std::string serialize() const;

    static ProposerEquivocationEvidence deserialize(const std::string& serialized);

private:
    std::string m_firstProposal;
    std::string m_secondProposal;
    std::string m_proposerAddress;
    std::uint64_t m_blockIndex;
    std::uint64_t m_round;
    std::string m_firstBlockHash;
    std::string m_secondBlockHash;
    std::int64_t m_detectedAt;
};

class SlashingEvidenceValidationResult {
public:
    SlashingEvidenceValidationResult();

    SlashingEvidenceValidationResult(
        SlashingEvidenceValidationStatus status,
        std::string reason,
        SlashingEvidenceRecord record
    );

    SlashingEvidenceValidationStatus status() const;
    const std::string& reason() const;
    const SlashingEvidenceRecord& record() const;

    bool accepted() const;
    bool duplicate() const;
    bool rejected() const;
    std::string serialize() const;

private:
    SlashingEvidenceValidationStatus m_status;
    std::string m_reason;
    SlashingEvidenceRecord m_record;
};

class SlashingEvidenceVerifier {
public:
    static SlashingEvidenceValidationResult verifyDoubleVoteEvidence(
        const DoubleVoteEvidence& evidence,
        const crypto::CryptoPolicy& policy,
        const crypto::SignatureProvider& provider
    );

    static SlashingEvidenceValidationResult validateDoubleVoteStructure(
        const DoubleVoteEvidence& evidence
    );

    static SlashingEvidenceValidationResult validateProposerEquivocationStructure(
        const ProposerEquivocationEvidence& evidence
    );

    static SlashingEvidenceValidationResult verifyDoubleVoteEvidenceForHistory(
        const DoubleVoteEvidence& evidence,
        std::uint64_t maximumOffenseHeight,
        const core::ValidatorSetHistory& validatorSetHistory,
        const crypto::CryptoPolicy& policy,
        const crypto::SignatureProvider& provider
    );

private:
    static bool bothVotesVerify(
        const DoubleVoteEvidence& evidence,
        const crypto::CryptoPolicy& policy,
        const crypto::SignatureProvider& provider
    );
};

} // namespace nodo::consensus

#endif
