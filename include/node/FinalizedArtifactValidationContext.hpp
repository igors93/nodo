#ifndef NODO_NODE_FINALIZED_ARTIFACT_VALIDATION_CONTEXT_HPP
#define NODO_NODE_FINALIZED_ARTIFACT_VALIDATION_CONTEXT_HPP

#include "config/NetworkParameters.hpp"
#include "core/StateTransitionPreview.hpp"
#include "crypto/ProtocolCryptoContext.hpp"
#include "node/FeeEconomics.hpp"
#include "node/FinalizedBlockArtifact.hpp"
#include "node/NodeRuntime.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace nodo::node {

class FinalizedArtifactValidationContext {
public:
    FinalizedArtifactValidationContext(
        const config::GenesisConfig& genesisConfig,
        NodeRuntime& runtime,
        const crypto::ProtocolCryptoContext& cryptoContext,
        std::filesystem::path blockPath,
        std::uint64_t requiredVoteCount,
        std::int64_t minimumFeeRawUnits
    );

    const config::GenesisConfig& genesisConfig() const;
    NodeRuntime& runtime();
    const NodeRuntime& runtime() const;
    const crypto::ProtocolCryptoContext& cryptoContext() const;
    const std::filesystem::path& blockPath() const;
    std::uint64_t requiredVoteCount() const;
    std::int64_t minimumFeeRawUnits() const;

    std::string rejectionPrefix() const;

    void setStatePreview(
        core::StateTransitionPreviewContext previewContext,
        core::StateTransitionPreviewResult preview
    );

    bool hasStatePreview() const;
    const core::StateTransitionPreviewContext& statePreviewContext() const;
    const core::StateTransitionPreviewResult& statePreview() const;

    void setExpectedFeeEconomicBalance(
        FeeEconomicBalance balance
    );

    bool hasExpectedFeeEconomicBalance() const;
    const FeeEconomicBalance& expectedFeeEconomicBalance() const;

    void setExpectedRewards(
        std::vector<RewardDistribution> rewards
    );

    const std::vector<RewardDistribution>& expectedRewards() const;

    void setExpectedLockedStakePositions(
        std::vector<LockedStakePosition> positions
    );

    const std::vector<LockedStakePosition>& expectedLockedStakePositions() const;

    void setExpectedSecurityScoreRecords(
        std::vector<SecurityScoreRecord> records
    );

    const std::vector<SecurityScoreRecord>& expectedSecurityScoreRecords() const;

    void setExpectedSecurityCheckpoints(
        std::vector<ValidatorSecurityCheckpoint> checkpoints
    );

    const std::vector<ValidatorSecurityCheckpoint>& expectedSecurityCheckpoints() const;

    void setExpectedRiskAssessments(
        std::vector<ValidatorRiskAssessment> assessments
    );

    const std::vector<ValidatorRiskAssessment>& expectedRiskAssessments() const;

    void setExpectedContainmentDecisions(
        std::vector<ValidatorContainmentDecision> decisions
    );

    const std::vector<ValidatorContainmentDecision>& expectedContainmentDecisions() const;

    void setExpectedNetworkPolicies(
        std::vector<ValidatorNetworkPolicy> policies
    );

    const std::vector<ValidatorNetworkPolicy>& expectedNetworkPolicies() const;

private:
    const config::GenesisConfig& m_genesisConfig;
    NodeRuntime& m_runtime;
    const crypto::ProtocolCryptoContext& m_cryptoContext;
    std::filesystem::path m_blockPath;
    std::uint64_t m_requiredVoteCount;
    std::int64_t m_minimumFeeRawUnits;
    std::optional<core::StateTransitionPreviewContext> m_previewContext;
    std::optional<core::StateTransitionPreviewResult> m_preview;
    std::optional<FeeEconomicBalance> m_expectedFeeBalance;
    std::optional<std::vector<RewardDistribution>> m_expectedRewards;
    std::optional<std::vector<LockedStakePosition>> m_expectedLockedStakePositions;
    std::optional<std::vector<SecurityScoreRecord>> m_expectedSecurityScoreRecords;
    std::optional<std::vector<ValidatorSecurityCheckpoint>> m_expectedSecurityCheckpoints;
    std::optional<std::vector<ValidatorRiskAssessment>> m_expectedRiskAssessments;
    std::optional<std::vector<ValidatorContainmentDecision>> m_expectedContainmentDecisions;
    std::optional<std::vector<ValidatorNetworkPolicy>> m_expectedNetworkPolicies;
};

} // namespace nodo::node

#endif
