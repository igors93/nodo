#include "node/FinalizedArtifactValidationContext.hpp"

#include <stdexcept>
#include <utility>

namespace nodo::node {

namespace {

template <typename T>
const T& requireCachedValue(
    const std::optional<T>& value,
    const char* name
) {
    if (!value.has_value()) {
        throw std::logic_error(std::string("Missing finalized artifact validation cache: ") + name);
    }

    return value.value();
}

} // namespace

FinalizedArtifactValidationContext::FinalizedArtifactValidationContext(
    const config::GenesisConfig& genesisConfig,
    NodeRuntime& runtime,
    const crypto::ProtocolCryptoContext& cryptoContext,
    std::filesystem::path blockPath,
    std::uint64_t requiredVoteCount,
    std::int64_t minimumFeeRawUnits
)
    : m_genesisConfig(genesisConfig),
      m_runtime(runtime),
      m_cryptoContext(cryptoContext),
      m_blockPath(std::move(blockPath)),
      m_requiredVoteCount(requiredVoteCount),
      m_minimumFeeRawUnits(minimumFeeRawUnits),
      m_previewContext(std::nullopt),
      m_preview(std::nullopt),
      m_expectedFeeBalance(std::nullopt),
      m_expectedRewards(std::nullopt),
      m_expectedLockedStakePositions(std::nullopt),
      m_expectedSecurityScoreRecords(std::nullopt),
      m_expectedSecurityCheckpoints(std::nullopt),
      m_expectedRiskAssessments(std::nullopt),
      m_expectedContainmentDecisions(std::nullopt),
      m_expectedNetworkPolicies(std::nullopt) {}

const config::GenesisConfig& FinalizedArtifactValidationContext::genesisConfig() const {
    return m_genesisConfig;
}

NodeRuntime& FinalizedArtifactValidationContext::runtime() {
    return m_runtime;
}

const NodeRuntime& FinalizedArtifactValidationContext::runtime() const {
    return m_runtime;
}

const crypto::ProtocolCryptoContext& FinalizedArtifactValidationContext::cryptoContext() const {
    return m_cryptoContext;
}

const std::filesystem::path& FinalizedArtifactValidationContext::blockPath() const {
    return m_blockPath;
}

std::uint64_t FinalizedArtifactValidationContext::requiredVoteCount() const {
    return m_requiredVoteCount;
}

std::int64_t FinalizedArtifactValidationContext::minimumFeeRawUnits() const {
    return m_minimumFeeRawUnits;
}

std::string FinalizedArtifactValidationContext::rejectionPrefix() const {
    return "Invalid finalized block file " + m_blockPath.string() + ": ";
}

void FinalizedArtifactValidationContext::setStatePreview(
    core::StateTransitionPreviewContext previewContext,
    core::StateTransitionPreviewResult preview
) {
    m_previewContext = std::move(previewContext);
    m_preview = std::move(preview);
}

bool FinalizedArtifactValidationContext::hasStatePreview() const {
    return m_preview.has_value();
}

const core::StateTransitionPreviewContext& FinalizedArtifactValidationContext::statePreviewContext() const {
    return requireCachedValue(m_previewContext, "statePreviewContext");
}

const core::StateTransitionPreviewResult& FinalizedArtifactValidationContext::statePreview() const {
    return requireCachedValue(m_preview, "statePreview");
}

void FinalizedArtifactValidationContext::setExpectedFeeEconomicBalance(
    FeeEconomicBalance balance
) {
    m_expectedFeeBalance = std::move(balance);
}

bool FinalizedArtifactValidationContext::hasExpectedFeeEconomicBalance() const {
    return m_expectedFeeBalance.has_value();
}

const FeeEconomicBalance& FinalizedArtifactValidationContext::expectedFeeEconomicBalance() const {
    return requireCachedValue(m_expectedFeeBalance, "expectedFeeEconomicBalance");
}

void FinalizedArtifactValidationContext::setExpectedRewards(
    std::vector<RewardDistribution> rewards
) {
    m_expectedRewards = std::move(rewards);
}

const std::vector<RewardDistribution>& FinalizedArtifactValidationContext::expectedRewards() const {
    return requireCachedValue(m_expectedRewards, "expectedRewards");
}

void FinalizedArtifactValidationContext::setExpectedLockedStakePositions(
    std::vector<LockedStakePosition> positions
) {
    m_expectedLockedStakePositions = std::move(positions);
}

const std::vector<LockedStakePosition>& FinalizedArtifactValidationContext::expectedLockedStakePositions() const {
    return requireCachedValue(m_expectedLockedStakePositions, "expectedLockedStakePositions");
}

void FinalizedArtifactValidationContext::setExpectedSecurityScoreRecords(
    std::vector<SecurityScoreRecord> records
) {
    m_expectedSecurityScoreRecords = std::move(records);
}

const std::vector<SecurityScoreRecord>& FinalizedArtifactValidationContext::expectedSecurityScoreRecords() const {
    return requireCachedValue(m_expectedSecurityScoreRecords, "expectedSecurityScoreRecords");
}

void FinalizedArtifactValidationContext::setExpectedSecurityCheckpoints(
    std::vector<ValidatorSecurityCheckpoint> checkpoints
) {
    m_expectedSecurityCheckpoints = std::move(checkpoints);
}

const std::vector<ValidatorSecurityCheckpoint>& FinalizedArtifactValidationContext::expectedSecurityCheckpoints() const {
    return requireCachedValue(m_expectedSecurityCheckpoints, "expectedSecurityCheckpoints");
}

void FinalizedArtifactValidationContext::setExpectedRiskAssessments(
    std::vector<ValidatorRiskAssessment> assessments
) {
    m_expectedRiskAssessments = std::move(assessments);
}

const std::vector<ValidatorRiskAssessment>& FinalizedArtifactValidationContext::expectedRiskAssessments() const {
    return requireCachedValue(m_expectedRiskAssessments, "expectedRiskAssessments");
}

void FinalizedArtifactValidationContext::setExpectedContainmentDecisions(
    std::vector<ValidatorContainmentDecision> decisions
) {
    m_expectedContainmentDecisions = std::move(decisions);
}

const std::vector<ValidatorContainmentDecision>& FinalizedArtifactValidationContext::expectedContainmentDecisions() const {
    return requireCachedValue(m_expectedContainmentDecisions, "expectedContainmentDecisions");
}

void FinalizedArtifactValidationContext::setExpectedNetworkPolicies(
    std::vector<ValidatorNetworkPolicy> policies
) {
    m_expectedNetworkPolicies = std::move(policies);
}

const std::vector<ValidatorNetworkPolicy>& FinalizedArtifactValidationContext::expectedNetworkPolicies() const {
    return requireCachedValue(m_expectedNetworkPolicies, "expectedNetworkPolicies");
}

} // namespace nodo::node

