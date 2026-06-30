#include "node/VerifiedSlashingEvidenceAdmission.hpp"

#include "core/ProtocolLimits.hpp"
#include "consensus/ProposerSchedule.hpp"
#include "node/SignedBlockProposalMessage.hpp"

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

consensus::SlashingEvidenceValidationResult
VerifiedSlashingEvidenceAdmission::admit(
    const consensus::ProposerEquivocationEvidence& evidence,
    std::uint64_t currentConsensusHeight,
    std::int64_t now,
    const core::ValidatorSetHistory& validatorSetHistory,
    const std::string& chainId,
    const crypto::CryptoPolicy& policy,
    const crypto::SignatureProvider& provider,
    consensus::EvidencePool& evidencePool
) {
    const consensus::SlashingEvidenceValidationResult structural =
        consensus::SlashingEvidenceVerifier::validateProposerEquivocationStructure(
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
    if (now <= 0 || evidence.detectedAt() > maximumTimestamp) {
        return consensus::SlashingEvidenceValidationResult(
            consensus::SlashingEvidenceValidationStatus::REJECTED,
            "Proposer-equivocation evidence contains a future timestamp.",
            structural.record()
        );
    }

    if (evidence.blockIndex() == 0 || evidence.blockIndex() > currentConsensusHeight) {
        return consensus::SlashingEvidenceValidationResult(
            consensus::SlashingEvidenceValidationStatus::REJECTED,
            "Proposer-equivocation evidence refers to an unavailable consensus height.",
            structural.record()
        );
    }
    if (chainId.empty() || !validatorSetHistory.hasSet(evidence.blockIndex())) {
        return consensus::SlashingEvidenceValidationResult(
            consensus::SlashingEvidenceValidationStatus::REJECTED,
            "Historical validator set is unavailable for proposer-equivocation evidence.",
            structural.record()
        );
    }

    node::SignedBlockProposalMessage firstProposal;
    node::SignedBlockProposalMessage secondProposal;
    try {
        firstProposal = node::SignedBlockProposalMessage::deserialize(evidence.firstProposal());
        secondProposal = node::SignedBlockProposalMessage::deserialize(evidence.secondProposal());
    } catch (const std::exception&) {
        return consensus::SlashingEvidenceValidationResult(
            consensus::SlashingEvidenceValidationStatus::REJECTED,
            "Proposer-equivocation evidence contains malformed signed proposals.",
            structural.record()
        );
    }

    if (firstProposal.proposerAddress() != evidence.proposerAddress() ||
        secondProposal.proposerAddress() != evidence.proposerAddress() ||
        firstProposal.blockIndex() != evidence.blockIndex() ||
        secondProposal.blockIndex() != evidence.blockIndex() ||
        firstProposal.round() != evidence.round() ||
        secondProposal.round() != evidence.round() ||
        firstProposal.blockHash() != evidence.firstBlockHash() ||
        secondProposal.blockHash() != evidence.secondBlockHash() ||
        firstProposal.blockHash() == secondProposal.blockHash() ||
        firstProposal.proposedAt() > maximumTimestamp ||
        secondProposal.proposedAt() > maximumTimestamp) {
        return consensus::SlashingEvidenceValidationResult(
            consensus::SlashingEvidenceValidationStatus::REJECTED,
            "Proposer-equivocation evidence metadata does not match signed proposals.",
            structural.record()
        );
    }

    const core::ValidatorRegistry& historicalValidators =
        validatorSetHistory.setAt(evidence.blockIndex());
    const std::string expectedProposer = consensus::ProposerSchedule::selectProposer(
        historicalValidators,
        chainId,
        evidence.blockIndex(),
        evidence.round()
    );
    if (expectedProposer.empty() || expectedProposer != evidence.proposerAddress()) {
        return consensus::SlashingEvidenceValidationResult(
            consensus::SlashingEvidenceValidationStatus::REJECTED,
            "Proposer-equivocation evidence signer was not the scheduled proposer.",
            structural.record()
        );
    }

    if (!firstProposal.verify(expectedProposer, historicalValidators, policy, provider) ||
        !secondProposal.verify(expectedProposer, historicalValidators, policy, provider)) {
        return consensus::SlashingEvidenceValidationResult(
            consensus::SlashingEvidenceValidationStatus::REJECTED,
            "Proposer-equivocation evidence contains an invalid proposal signature.",
            structural.record()
        );
    }

    return evidencePool.submitProposerEquivocationEvidence(
        consensus::ProposerEquivocationEvidence(
            evidence.firstProposal(),
            evidence.secondProposal(),
            evidence.proposerAddress(),
            evidence.blockIndex(),
            evidence.round(),
            evidence.firstBlockHash(),
            evidence.secondBlockHash(),
            now
        )
    );
}

} // namespace nodo::node
