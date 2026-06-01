#include "node/ValidatorLifecycleArtifactValidator.hpp"

#include "node/ValidatorLifecycle.hpp"

#include <exception>
#include <vector>

namespace nodo::node {

ArtifactValidationResult ValidatorLifecycleArtifactValidator::validate(
    FinalizedArtifactValidationContext& context,
    const FinalizedBlockArtifact& artifact
) {
    const std::string prefix =
        context.rejectionPrefix();

    try {
        const core::Block& block =
            artifact.block();

        const std::vector<ValidatorLifecycleRecord> expectedLifecycleRecords =
            ValidatorLifecycle::buildLifecycleRecords(
                block.index(),
                context.expectedRewards(),
                context.expectedLockedStakePositions(),
                context.expectedSecurityScoreRecords(),
                artifact.protectionRewardSettlements(),
                artifact.stakePenaltyRecords()
            );

        if (!ValidatorLifecycle::sameLifecycleRecords(expectedLifecycleRecords, artifact.validatorLifecycleRecords())) {
            return ArtifactValidationResult::rejected(
                prefix + "Persisted validator lifecycle records do not match rebuilt accounting."
            );
        }

        const EpochAccountingRecord expectedEpochAccounting =
            ValidatorLifecycle::buildEpochAccountingRecord(
                block.index(),
                expectedLifecycleRecords
            );

        if (!ValidatorLifecycle::sameEpochAccounting(expectedEpochAccounting, artifact.epochAccountingRecord())) {
            return ArtifactValidationResult::rejected(
                prefix + "Persisted epoch accounting does not match rebuilt validator lifecycle."
            );
        }

        const ValidatorLifecycleSummary expectedLifecycleSummary =
            ValidatorLifecycle::buildSummary(
                block.index(),
                expectedLifecycleRecords,
                expectedEpochAccounting
            );

        if (!ValidatorLifecycle::sameSummary(expectedLifecycleSummary, artifact.validatorLifecycleSummary())) {
            return ArtifactValidationResult::rejected(
                prefix + "Persisted validator lifecycle summary does not match rebuilt epoch accounting."
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

