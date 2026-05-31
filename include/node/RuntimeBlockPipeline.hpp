#ifndef NODO_NODE_RUNTIME_BLOCK_PIPELINE_HPP
#define NODO_NODE_RUNTIME_BLOCK_PIPELINE_HPP

#include "consensus/BlockFinalizer.hpp"
#include "consensus/QuorumCertificate.hpp"
#include "core/Block.hpp"
#include "core/MempoolBlockProducer.hpp"
#include "crypto/Signer.hpp"
#include "node/LockedStakePosition.hpp"
#include "node/MonetaryFirewall.hpp"
#include "node/NodeRuntime.hpp"
#include "node/RewardDistribution.hpp"
#include "node/SecurityScore.hpp"
#include "node/SecurityCheckpoint.hpp"
#include "node/ValidatorRiskAssessment.hpp"
#include "node/ValidatorContainmentDecision.hpp"
#include "node/ValidatorNetworkPolicy.hpp"
#include "utils/Amount.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nodo::node {

/*
 * RuntimeBlockPipelineConfig controls one local block production/finalization
 * attempt.
 *
 * Security principle:
 * A runtime pipeline must not create a block unless the mempool, validator
 * registry, quorum certificate and finalization registry all agree.
 */
class RuntimeBlockPipelineConfig {
public:
    RuntimeBlockPipelineConfig();

    RuntimeBlockPipelineConfig(
        std::size_t maxTransactionsPerBlock,
        std::size_t minTransactionsPerBlock,
        std::uint64_t consensusRound,
        std::int64_t timestamp
    );

    std::size_t maxTransactionsPerBlock() const;
    std::size_t minTransactionsPerBlock() const;
    std::uint64_t consensusRound() const;
    std::int64_t timestamp() const;

    bool isValid() const;
    std::string serialize() const;

private:
    std::size_t m_maxTransactionsPerBlock;
    std::size_t m_minTransactionsPerBlock;
    std::uint64_t m_consensusRound;
    std::int64_t m_timestamp;
};

enum class RuntimeBlockPipelineStatus {
    FINALIZED,
    INVALID_CONFIG,
    INVALID_RUNTIME,
    BLOCK_PRODUCTION_FAILED,
    STATE_TRANSITION_FAILED,
    NOT_ENOUGH_VALIDATORS,
    VOTE_BUILD_FAILED,
    QUORUM_BUILD_FAILED,
    FINALIZATION_FAILED
};

std::string runtimeBlockPipelineStatusToString(
    RuntimeBlockPipelineStatus status
);

class RuntimeBlockPipelineResult {
public:
    RuntimeBlockPipelineResult();

    static RuntimeBlockPipelineResult finalized(
        core::Block block,
        consensus::QuorumCertificate certificate,
        consensus::FinalizedBlockRecord finalizedRecord,
        std::vector<std::string> finalizedTransactionIds,
        std::string postStateRoot
    );

    static RuntimeBlockPipelineResult finalized(
        core::Block block,
        consensus::QuorumCertificate certificate,
        consensus::FinalizedBlockRecord finalizedRecord,
        std::vector<std::string> finalizedTransactionIds,
        std::string postStateRoot,
        utils::Amount totalFee
    );

    static RuntimeBlockPipelineResult finalized(
        core::Block block,
        consensus::QuorumCertificate certificate,
        consensus::FinalizedBlockRecord finalizedRecord,
        std::vector<std::string> finalizedTransactionIds,
        std::string postStateRoot,
        utils::Amount totalFee,
        std::vector<RewardDistribution> rewardDistributions
    );

    static RuntimeBlockPipelineResult finalized(
        core::Block block,
        consensus::QuorumCertificate certificate,
        consensus::FinalizedBlockRecord finalizedRecord,
        std::vector<std::string> finalizedTransactionIds,
        std::string postStateRoot,
        utils::Amount totalFee,
        std::vector<RewardDistribution> rewardDistributions,
        std::vector<LockedStakePosition> lockedStakePositions
    );

    static RuntimeBlockPipelineResult finalized(
        core::Block block,
        consensus::QuorumCertificate certificate,
        consensus::FinalizedBlockRecord finalizedRecord,
        std::vector<std::string> finalizedTransactionIds,
        std::string postStateRoot,
        utils::Amount totalFee,
        std::vector<RewardDistribution> rewardDistributions,
        std::vector<LockedStakePosition> lockedStakePositions,
        std::vector<SecurityScoreRecord> securityScoreRecords
    );

    static RuntimeBlockPipelineResult finalized(
        core::Block block,
        consensus::QuorumCertificate certificate,
        consensus::FinalizedBlockRecord finalizedRecord,
        std::vector<std::string> finalizedTransactionIds,
        std::string postStateRoot,
        utils::Amount totalFee,
        std::vector<RewardDistribution> rewardDistributions,
        std::vector<LockedStakePosition> lockedStakePositions,
        std::vector<SecurityScoreRecord> securityScoreRecords,
        std::vector<ValidatorSecurityCheckpoint> securityCheckpoints
    );

    static RuntimeBlockPipelineResult finalized(
        core::Block block,
        consensus::QuorumCertificate certificate,
        consensus::FinalizedBlockRecord finalizedRecord,
        std::vector<std::string> finalizedTransactionIds,
        std::string postStateRoot,
        utils::Amount totalFee,
        std::vector<RewardDistribution> rewardDistributions,
        std::vector<LockedStakePosition> lockedStakePositions,
        std::vector<SecurityScoreRecord> securityScoreRecords,
        std::vector<ValidatorSecurityCheckpoint> securityCheckpoints,
        std::vector<ValidatorRiskAssessment> validatorRiskAssessments
    );

    static RuntimeBlockPipelineResult finalized(
        core::Block block,
        consensus::QuorumCertificate certificate,
        consensus::FinalizedBlockRecord finalizedRecord,
        std::vector<std::string> finalizedTransactionIds,
        std::string postStateRoot,
        utils::Amount totalFee,
        std::vector<RewardDistribution> rewardDistributions,
        std::vector<LockedStakePosition> lockedStakePositions,
        std::vector<SecurityScoreRecord> securityScoreRecords,
        std::vector<ValidatorSecurityCheckpoint> securityCheckpoints,
        std::vector<ValidatorRiskAssessment> validatorRiskAssessments,
        std::vector<ValidatorContainmentDecision> validatorContainmentDecisions
    );

    static RuntimeBlockPipelineResult finalized(
        core::Block block,
        consensus::QuorumCertificate certificate,
        consensus::FinalizedBlockRecord finalizedRecord,
        std::vector<std::string> finalizedTransactionIds,
        std::string postStateRoot,
        utils::Amount totalFee,
        std::vector<RewardDistribution> rewardDistributions,
        std::vector<LockedStakePosition> lockedStakePositions,
        std::vector<SecurityScoreRecord> securityScoreRecords,
        std::vector<ValidatorSecurityCheckpoint> securityCheckpoints,
        std::vector<ValidatorRiskAssessment> validatorRiskAssessments,
        std::vector<ValidatorContainmentDecision> validatorContainmentDecisions,
        std::vector<ValidatorNetworkPolicy> validatorNetworkPolicies
    );

    static RuntimeBlockPipelineResult finalized(
        core::Block block,
        consensus::QuorumCertificate certificate,
        consensus::FinalizedBlockRecord finalizedRecord,
        std::vector<std::string> finalizedTransactionIds,
        std::string postStateRoot,
        utils::Amount totalFee,
        std::vector<RewardDistribution> rewardDistributions,
        std::vector<LockedStakePosition> lockedStakePositions,
        std::vector<SecurityScoreRecord> securityScoreRecords,
        std::vector<ValidatorSecurityCheckpoint> securityCheckpoints,
        std::vector<ValidatorRiskAssessment> validatorRiskAssessments,
        std::vector<ValidatorContainmentDecision> validatorContainmentDecisions,
        std::vector<ValidatorNetworkPolicy> validatorNetworkPolicies,
        MonetaryFirewallAudit monetaryFirewallAudit
    );

    static RuntimeBlockPipelineResult rejected(
        RuntimeBlockPipelineStatus status,
        std::string reason
    );

    RuntimeBlockPipelineStatus status() const;
    const std::string& reason() const;
    bool finalized() const;

    const core::Block& block() const;
    const consensus::QuorumCertificate& certificate() const;
    const consensus::FinalizedBlockRecord& finalizedRecord() const;
    const std::vector<std::string>& finalizedTransactionIds() const;
    const std::string& postStateRoot() const;
    utils::Amount totalFee() const;
    const std::vector<RewardDistribution>& rewardDistributions() const;
    const std::vector<LockedStakePosition>& lockedStakePositions() const;
    const std::vector<SecurityScoreRecord>& securityScoreRecords() const;
    const std::vector<ValidatorSecurityCheckpoint>& securityCheckpoints() const;
    const std::vector<ValidatorRiskAssessment>& validatorRiskAssessments() const;
    const std::vector<ValidatorContainmentDecision>& validatorContainmentDecisions() const;
    const std::vector<ValidatorNetworkPolicy>& validatorNetworkPolicies() const;
    const MonetaryFirewallAudit& monetaryFirewallAudit() const;

    std::string serialize() const;

private:
    RuntimeBlockPipelineStatus m_status;
    std::string m_reason;

    /*
     * Block has no default constructor in the current core model.
     * Optional keeps rejected results representable without creating a fake block.
     */
    std::optional<core::Block> m_block;

    consensus::QuorumCertificate m_certificate;
    consensus::FinalizedBlockRecord m_finalizedRecord;
    std::vector<std::string> m_finalizedTransactionIds;
    std::string m_postStateRoot;
    utils::Amount m_totalFee;
    std::vector<RewardDistribution> m_rewardDistributions;
    std::vector<LockedStakePosition> m_lockedStakePositions;
    std::vector<SecurityScoreRecord> m_securityScoreRecords;
    std::vector<ValidatorSecurityCheckpoint> m_securityCheckpoints;
    std::vector<ValidatorRiskAssessment> m_validatorRiskAssessments;
    std::vector<ValidatorContainmentDecision> m_validatorContainmentDecisions;
    std::vector<ValidatorNetworkPolicy> m_validatorNetworkPolicies;
    MonetaryFirewallAudit m_monetaryFirewallAudit;
};

/*
 * RuntimeBlockPipeline connects the local pieces already implemented:
 *
 *   Mempool -> MempoolBlockProducer -> Validator votes -> QuorumCertificate
 *   -> BlockFinalizer -> mempool cleanup.
 *
 * This is still a local runtime pipeline. P2P propagation and remote validator
 * networking are intentionally outside this class.
 */
class RuntimeBlockPipeline {
public:
    static RuntimeBlockPipelineResult produceAndFinalizeNextBlock(
        NodeRuntime& runtime,
        const RuntimeBlockPipelineConfig& config,
        const crypto::Signer& localValidatorSigner
    );

private:
    static std::vector<consensus::ValidatorVoteRecord> buildValidatorVotes(
        const NodeRuntime& runtime,
        const core::Block& block,
        std::uint64_t consensusRound,
        std::int64_t timestamp,
        const crypto::Signer& localValidatorSigner
    );

    static void removeFinalizedTransactionsFromMempool(
        NodeRuntime& runtime,
        const std::vector<std::string>& transactionIds
    );
};

} // namespace nodo::node

#endif
