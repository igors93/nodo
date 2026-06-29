#include "node/GovernanceArtifactValidator.hpp"

#include "node/Governance.hpp"
#include "node/ProtocolStateTransition.hpp"

#include <exception>
#include <vector>

namespace nodo::node {

ArtifactValidationResult GovernanceArtifactValidator::validate(
    FinalizedArtifactValidationContext& context,
    const FinalizedBlockArtifact& artifact
) {
    const std::string prefix =
        context.rejectionPrefix();

    try {
        const core::Block& block =
            artifact.block();

        const GovernancePolicySnapshot expectedGovernancePolicy =
            Governance::buildPolicySnapshot(block.index());

        if (!Governance::samePolicy(expectedGovernancePolicy, artifact.governancePolicySnapshot())) {
            return ArtifactValidationResult::rejected(
                prefix + "Persisted governance policy does not match protocol policy."
            );
        }

        const std::vector<GovernanceActionGuard> expectedGovernanceGuards =
            Governance::buildActionGuards(expectedGovernancePolicy);

        if (!Governance::sameActionGuards(expectedGovernanceGuards, artifact.governanceActionGuards())) {
            return ArtifactValidationResult::rejected(
                prefix + "Persisted governance guards do not match protocol policy."
            );
        }

        const ProtocolExecutionState replayed =
            ProtocolStateTransition::replayFinalizedBlockDomains(
                context.runtime(),
                block
            );
        const GovernanceSummary expectedGovernanceSummary =
            Governance::buildSummary(
                block.index(),
                expectedGovernanceGuards,
                static_cast<std::uint64_t>(replayed.governance.activeProposalCount()),
                static_cast<std::uint64_t>(replayed.governance.approvedProposalCount()),
                static_cast<std::uint64_t>(replayed.governance.executableProposalCount(block.index() + 1)),
                static_cast<std::uint64_t>(replayed.governance.executedProposalCount()),
                replayed.governance.serialize()
            );

        if (!Governance::sameSummary(expectedGovernanceSummary, artifact.governanceSummary())) {
            return ArtifactValidationResult::rejected(
                prefix + "Persisted governance summary does not match governance guards."
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
