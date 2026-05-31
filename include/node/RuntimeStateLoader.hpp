#ifndef NODO_NODE_RUNTIME_STATE_LOADER_HPP
#define NODO_NODE_RUNTIME_STATE_LOADER_HPP

#include "config/NetworkParameters.hpp"
#include "consensus/BlockFinalizer.hpp"
#include "consensus/QuorumCertificate.hpp"
#include "core/Block.hpp"
#include "node/LockedStakePosition.hpp"
#include "node/MonetaryFirewall.hpp"
#include "node/ProtectionTreasury.hpp"
#include "node/ControlledIssuance.hpp"
#include "node/FeeEconomics.hpp"
#include "node/NodeDataDirectory.hpp"
#include "node/NodeRuntime.hpp"
#include "node/RewardDistribution.hpp"
#include "node/SecurityScore.hpp"
#include "node/SecurityCheckpoint.hpp"
#include "node/ValidatorRiskAssessment.hpp"
#include "node/ValidatorContainmentDecision.hpp"
#include "node/ValidatorNetworkPolicy.hpp"
#include "p2p/PeerMessage.hpp"
#include "utils/Amount.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace nodo::node {

enum class RuntimeStateLoadStatus {
    LOADED,
    INVALID_CONFIG,
    NOT_INITIALIZED,
    GENESIS_MISMATCH,
    RUNTIME_START_FAILED,
    BLOCK_FILE_MISSING,
    BLOCK_FILE_INVALID,
    BLOCK_APPEND_FAILED,
    MANIFEST_MISMATCH,
    MEMPOOL_LOAD_FAILED
};

std::string runtimeStateLoadStatusToString(
    RuntimeStateLoadStatus status
);

class RuntimeStateLoadResult {
public:
    RuntimeStateLoadResult();

    static RuntimeStateLoadResult loaded(
        NodeRuntime runtime,
        NodeRuntimeManifest manifest,
        std::size_t loadedBlockCount,
        std::size_t loadedMempoolTransactionCount
    );

    static RuntimeStateLoadResult rejected(
        RuntimeStateLoadStatus status,
        std::string reason
    );

    RuntimeStateLoadStatus status() const;
    const std::string& reason() const;
    bool loaded() const;

    const NodeRuntime& runtime() const;
    const NodeRuntimeManifest& manifest() const;
    std::size_t loadedBlockCount() const;
    std::size_t loadedMempoolTransactionCount() const;

    std::string serialize() const;

private:
    RuntimeStateLoadStatus m_status;
    std::string m_reason;
    NodeRuntime m_runtime;
    NodeRuntimeManifest m_manifest;
    std::size_t m_loadedBlockCount;
    std::size_t m_loadedMempoolTransactionCount;
};

class FinalizedBlockArtifact {
public:
    FinalizedBlockArtifact();

    FinalizedBlockArtifact(
        core::Block block,
        std::string postStateRoot,
        utils::Amount totalFee,
        std::vector<RewardDistribution> rewardDistributions,
        std::vector<LockedStakePosition> lockedStakePositions,
        std::vector<SecurityScoreRecord> securityScoreRecords,
        std::vector<ValidatorSecurityCheckpoint> securityCheckpoints,
        std::vector<ValidatorRiskAssessment> validatorRiskAssessments,
        std::vector<ValidatorContainmentDecision> validatorContainmentDecisions,
        std::vector<ValidatorNetworkPolicy> validatorNetworkPolicies,
        MonetaryFirewallAudit monetaryFirewallAudit,
        GenesisTreasurySnapshot genesisTreasurySnapshot,
        ProtectionRewardBudget protectionRewardBudget,
        std::vector<ProtectionRewardGrant> protectionRewardGrants,
        InflationEpochSnapshot inflationEpochSnapshot,
        MintAuthorizationRecord mintAuthorizationRecord,
        SupplyExpansionRecord supplyExpansionRecord,
        FeeEconomicBalance feeEconomicBalance,
        FeeBurnRecord feeBurnRecord,
        TreasuryFeeRecord treasuryFeeRecord,
        consensus::QuorumCertificate quorumCertificate,
        consensus::FinalizedBlockRecord finalizedRecord
    );

    const core::Block& block() const;
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
    const GenesisTreasurySnapshot& genesisTreasurySnapshot() const;
    const ProtectionRewardBudget& protectionRewardBudget() const;
    const std::vector<ProtectionRewardGrant>& protectionRewardGrants() const;
    const InflationEpochSnapshot& inflationEpochSnapshot() const;
    const MintAuthorizationRecord& mintAuthorizationRecord() const;
    const SupplyExpansionRecord& supplyExpansionRecord() const;
    const FeeEconomicBalance& feeEconomicBalance() const;
    const FeeBurnRecord& feeBurnRecord() const;
    const TreasuryFeeRecord& treasuryFeeRecord() const;
    const consensus::QuorumCertificate& quorumCertificate() const;
    const consensus::FinalizedBlockRecord& finalizedRecord() const;

    bool isValid() const;
    std::string serialize() const;

private:
    std::optional<core::Block> m_block;
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
    GenesisTreasurySnapshot m_genesisTreasurySnapshot;
    ProtectionRewardBudget m_protectionRewardBudget;
    std::vector<ProtectionRewardGrant> m_protectionRewardGrants;
    InflationEpochSnapshot m_inflationEpochSnapshot;
    MintAuthorizationRecord m_mintAuthorizationRecord;
    SupplyExpansionRecord m_supplyExpansionRecord;
    FeeEconomicBalance m_feeEconomicBalance;
    FeeBurnRecord m_feeBurnRecord;
    TreasuryFeeRecord m_treasuryFeeRecord;
    consensus::QuorumCertificate m_quorumCertificate;
    consensus::FinalizedBlockRecord m_finalizedRecord;
};

class FinalizedBlockFileCodec {
public:
    static core::Block readBlockFile(
        const std::filesystem::path& path
    );

    static core::Block decodeBlockFileContents(
        const std::string& contents
    );

    static FinalizedBlockArtifact readBlockArtifactFile(
        const std::filesystem::path& path
    );

    static FinalizedBlockArtifact decodeBlockArtifactFileContents(
        const std::string& contents
    );
};

class RuntimeStateLoader {
public:
    static RuntimeStateLoadResult loadFromDataDirectory(
        const NodeDataDirectoryConfig& directoryConfig,
        const config::GenesisConfig& genesisConfig,
        const p2p::PeerInfo& localPeer
    );
};

} // namespace nodo::node

#endif
