#include "node/SlashingArtifactValidator.hpp"

#include "node/CryptographicSlashing.hpp"
#include "node/SlashingEvidence.hpp"

#include <exception>
#include <vector>

namespace nodo::node {

ArtifactValidationResult SlashingArtifactValidator::validate(
    FinalizedArtifactValidationContext& context,
    const FinalizedBlockArtifact& artifact
) {
    const std::string prefix =
        context.rejectionPrefix();

    try {
        const core::Block& block =
            artifact.block();

        const std::vector<SlashingEvidenceRecord> expectedSlashingEvidence =
            SlashingEvidence::buildEvidenceRecords(
                context.expectedRiskAssessments(),
                context.expectedNetworkPolicies(),
                artifact.protectionWorkRecords()
            );

        if (!SlashingEvidence::sameEvidenceRecords(expectedSlashingEvidence, artifact.slashingEvidenceRecords())) {
            return ArtifactValidationResult::rejected(
                prefix + "Persisted slashing evidence records do not match rebuilt security evidence."
            );
        }

        const std::vector<SlashingPreparationRecord> expectedSlashingPreparation =
            SlashingEvidence::buildPreparationRecords(
                expectedSlashingEvidence,
                context.expectedLockedStakePositions()
            );

        if (!SlashingEvidence::samePreparationRecords(expectedSlashingPreparation, artifact.slashingPreparationRecords())) {
            return ArtifactValidationResult::rejected(
                prefix + "Persisted slashing preparation records do not match rebuilt evidence."
            );
        }

        const SlashingEvidenceSummary expectedSlashingSummary =
            SlashingEvidence::buildSummary(
                block.index(),
                expectedSlashingEvidence,
                expectedSlashingPreparation
            );

        if (!SlashingEvidence::sameSummary(expectedSlashingSummary, artifact.slashingEvidenceSummary())) {
            return ArtifactValidationResult::rejected(
                prefix + "Persisted slashing evidence summary does not match rebuilt evidence."
            );
        }

        const std::vector<CryptographicSlashingEvidenceRecord> expectedCryptographicEvidence =
            CryptographicSlashing::buildEvidenceRecordsFromCertifiedVotes(
                artifact.quorumCertificate().votes()
            );

        if (!CryptographicSlashing::sameEvidenceRecords(expectedCryptographicEvidence, artifact.cryptographicSlashingEvidenceRecords())) {
            return ArtifactValidationResult::rejected(
                prefix + "Persisted cryptographic slashing evidence does not match rebuilt vote evidence."
            );
        }

        const std::vector<StakePenaltyRecord> expectedStakePenalties =
            CryptographicSlashing::buildStakePenaltyRecords(
                expectedCryptographicEvidence,
                context.expectedLockedStakePositions()
            );

        if (!CryptographicSlashing::sameStakePenaltyRecords(expectedStakePenalties, artifact.stakePenaltyRecords())) {
            return ArtifactValidationResult::rejected(
                prefix + "Persisted stake penalty records do not match rebuilt cryptographic evidence."
            );
        }

        const CryptographicSlashingSummary expectedCryptographicSummary =
            CryptographicSlashing::buildSummary(
                block.index(),
                expectedCryptographicEvidence,
                expectedStakePenalties
            );

        if (!CryptographicSlashing::sameSummary(expectedCryptographicSummary, artifact.cryptographicSlashingSummary())) {
            return ArtifactValidationResult::rejected(
                prefix + "Persisted cryptographic slashing summary does not match rebuilt evidence."
            );
        }
    } catch (const std::exception& error) {
        return ArtifactValidationResult::rejected(
            prefix + error.what()
        );
    }

    return ArtifactValidationResult::acceptedResult();
}

} // namespace nodo::node

