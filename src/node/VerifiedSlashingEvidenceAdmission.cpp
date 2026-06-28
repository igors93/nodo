#include "node/VerifiedSlashingEvidenceAdmission.hpp"

#include "core/ProtocolLimits.hpp"

#include <limits>

namespace nodo::node {

consensus::SlashingEvidenceValidationResult
VerifiedSlashingEvidenceAdmission::admit(
    const consensus::DoubleVoteEvidence& evidence,
    std::uint64_t currentConsensusHeight,
    std::int64_t now,
    const core::ValidatorSetHistory& validatorSetHistory,
    const crypto::CryptoPolicy& policy,
    const crypto::SignatureProvider& provider,
    consensus::EvidencePool& evidencePool
) {
    const consensus::SlashingEvidenceValidationResult structural =
        consensus::SlashingEvidenceVerifier::validateDoubleVoteStructure(
            evidence
        );
    if (!structural.accepted()) {
        return structural;
    }
    if (evidencePool.contains(evidence.evidenceId())) {
        return consensus::SlashingEvidenceValidationResult(
            consensus::SlashingEvidenceValidationStatus::DUPLICATE,
            "Slashing evidence is already known.",
            structural.record()
        );
    }

    const std::int64_t skew =
        core::ProtocolLimits::MAX_SLASHING_EVIDENCE_CLOCK_SKEW_SECONDS;
    const std::int64_t maximumTimestamp =
        now > std::numeric_limits<std::int64_t>::max() - skew
            ? std::numeric_limits<std::int64_t>::max()
            : now + skew;
    if (now <= 0 || evidence.detectedAt() > maximumTimestamp ||
        evidence.firstVote().createdAt() > maximumTimestamp ||
        evidence.secondVote().createdAt() > maximumTimestamp) {
        return consensus::SlashingEvidenceValidationResult(
            consensus::SlashingEvidenceValidationStatus::REJECTED,
            "Slashing evidence contains a future timestamp.",
            structural.record()
        );
    }

    const consensus::SlashingEvidenceValidationResult verified =
        consensus::SlashingEvidenceVerifier::
            verifyDoubleVoteEvidenceForHistory(
                evidence,
                currentConsensusHeight,
                validatorSetHistory,
                policy,
                provider
            );
    if (!verified.accepted()) {
        return verified;
    }

    return evidencePool.submitDoubleVoteEvidence(
        consensus::DoubleVoteEvidence(
            evidence.firstVote(), evidence.secondVote(), now
        )
    );
}

} // namespace nodo::node
