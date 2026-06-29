#ifndef NODO_NODE_NODE_RUNTIME_HPP
#define NODO_NODE_NODE_RUNTIME_HPP

#include "config/NetworkParameters.hpp"
#include "consensus/BlockFinalizer.hpp"
#include "consensus/ConsensusRoundManager.hpp"
#include "consensus/ForkChoice.hpp"
#include "consensus/ValidatorPenaltyApplication.hpp"
#include "core/AccountStateView.hpp"
#include "core/Blockchain.hpp"
#include "core/StatePruner.hpp"
#include "core/ValidatorRegistry.hpp"
#include "mempool/Mempool.hpp"
#include "node/GovernanceExecutor.hpp"
#include "node/RuntimeSupplyState.hpp"
#include "node/StakingRegistry.hpp"
#include "p2p/LocalNodeSync.hpp"
#include "p2p/PeerMessage.hpp"

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace nodo::node {

enum class PeerUpdateStatus {
    ACCEPTED,
    UPDATED,
    DUPLICATE,
    REJECTED,
    CAPACITY_REACHED
};

std::string peerUpdateStatusToString(
    PeerUpdateStatus status
);

class PeerUpdateResult {
public:
    PeerUpdateResult();

    static PeerUpdateResult accepted(
        p2p::PeerInfo peer
    );

    static PeerUpdateResult updated(
        p2p::PeerInfo peer
    );

    static PeerUpdateResult duplicate(
        p2p::PeerInfo peer
    );

    static PeerUpdateResult rejected(
        PeerUpdateStatus status,
        std::string reason
    );

    PeerUpdateStatus status() const;
    const std::string& reason() const;
    const p2p::PeerInfo& peer() const;

    bool success() const;
    std::string serialize() const;

private:
    PeerUpdateStatus m_status;
    std::string m_reason;
    p2p::PeerInfo m_peer;
};

class LocalPeerManager {
public:
    explicit LocalPeerManager(
        std::size_t maxPeers = 128
    );

    PeerUpdateResult addOrUpdatePeer(
        const p2p::PeerInfo& peer
    );

    bool removePeer(
        const std::string& peerId
    );

    bool hasPeer(
        const std::string& peerId
    ) const;

    const p2p::PeerInfo* peer(
        const std::string& peerId
    ) const;

    const p2p::PeerInfo* bestSyncPeer(
        std::uint64_t localHeight
    ) const;

    std::vector<p2p::PeerInfo> peers() const;

    std::size_t size() const;
    std::size_t maxPeers() const;

    bool isValid() const;
    std::string serialize() const;

private:
    std::size_t m_maxPeers;
    std::map<std::string, p2p::PeerInfo> m_peersById;
};

enum class NodeRuntimeStatus {
    STOPPED,
    RUNNING,
    HALTED
};

std::string nodeRuntimeStatusToString(
    NodeRuntimeStatus status
);

class NodeRuntimeConfig {
public:
    NodeRuntimeConfig();

    NodeRuntimeConfig(
        config::GenesisConfig genesisConfig,
        p2p::PeerInfo localPeer,
        std::size_t maxPeers
    );

    const config::GenesisConfig& genesisConfig() const;
    const p2p::PeerInfo& localPeer() const;
    std::size_t maxPeers() const;

    bool isValid() const;
    std::string serialize() const;

private:
    config::GenesisConfig m_genesisConfig;
    p2p::PeerInfo m_localPeer;
    std::size_t m_maxPeers;
};

class NodeRuntime {
public:
    NodeRuntime();

    NodeRuntime(
        NodeRuntimeConfig config,
        core::Blockchain blockchain,
        core::ValidatorRegistry validatorRegistry
    );

    const NodeRuntimeConfig& config() const;
    NodeRuntimeStatus status() const;
    const core::Blockchain& blockchain() const;
    core::Blockchain& mutableBlockchain();
    const core::ValidatorRegistry& validatorRegistry() const;
    core::ValidatorRegistry& mutableValidatorRegistry();
    const core::ValidatorSetHistory& validatorSetHistory() const;
    core::ValidatorSetHistory& mutableValidatorSetHistory();
    const consensus::BlockFinalizationRegistry& finalizationRegistry() const;
    consensus::BlockFinalizationRegistry& mutableFinalizationRegistry();
    const consensus::ConsensusRoundManager& consensusRoundManager() const;
    consensus::ConsensusRoundManager& mutableConsensusRoundManager();
    const mempool::Mempool& mempool() const;
    mempool::Mempool& mutableMempool();
    const LocalPeerManager& peerManager() const;
    LocalPeerManager& mutablePeerManager();
    const RuntimeSupplyState& supplyState() const;
    RuntimeSupplyState& mutableSupplyState();
    const core::StatePruner& statePruner() const;
    core::StatePruner& mutableStatePruner();
    const consensus::ValidatorPenaltyLedger& validatorPenaltyLedger() const;
    consensus::ValidatorPenaltyLedger& mutableValidatorPenaltyLedger();
    const StakingRegistry& stakingRegistry() const;
    StakingRegistry& mutableStakingRegistry();

    // Return a lazily-built account state view for the current chain tip.
    // The view is cached by tip height; adding a block via mutableBlockchain()
    // must be followed by invalidateAccountStateCache() to force a rebuild.
    // minimumFeeRawUnits is only used on a cache miss (rebuild).
    const core::AccountStateView& cachedAccountStateAtTip(
        std::int64_t minimumFeeRawUnits
    ) const;

    // Discard the cached account state so the next call to
    // cachedAccountStateAtTip() rebuilds from the updated chain.
    void invalidateAccountStateCache();

    // Access the governance executor that tracks applied parameter changes.
    const GovernanceExecutor& governanceExecutor() const;
    GovernanceExecutor& mutableGovernanceExecutor();

    // Return the effective minimum fee raw units, accounting for any applied
    // governance overrides on top of the genesis network parameters.
    std::uint64_t effectiveMinimumFeeRawUnits() const;

    // Process any GOVERNANCE_PROPOSE transactions in the given block and call
    // executeProposal() for each, so governance changes take effect.
    void applyGovernanceFromBlock(const core::Block& block, std::int64_t now);

    void applySlashingEvidenceFromBlock(const core::Block& block);

    bool isRunning() const;
    bool isHalted() const;
    bool isValid() const;
    void halt();

    consensus::ChainForkSummary chainSummary() const;

    consensus::VoteCollectResult submitConsensusVote(
        const consensus::ValidatorVoteRecord& vote
    );

    bool advanceConsensusRoundIfTimedOut(
        std::int64_t now
    );

    p2p::PeerMessage localChainSummaryMessage(
        const std::string& toPeerId,
        std::int64_t createdAt
    ) const;

    p2p::LocalSyncPlan evaluatePeerForSync(
        const p2p::PeerInfo& remotePeer,
        const consensus::ChainForkSummary& remoteSummary,
        std::int64_t createdAt
    ) const;

    std::string serialize() const;

private:
    NodeRuntimeConfig m_config;
    NodeRuntimeStatus m_status;
    core::Blockchain m_blockchain;
    core::ValidatorRegistry m_validatorRegistry;
    core::ValidatorSetHistory m_validatorSetHistory;
    consensus::BlockFinalizationRegistry m_finalizationRegistry;
    consensus::ConsensusRoundManager m_consensusRoundManager;
    mempool::Mempool m_mempool;
    LocalPeerManager m_peerManager;
    RuntimeSupplyState m_supplyState;
    core::StatePruner m_statePruner;
    GovernanceExecutor m_governanceExecutor;
    consensus::ValidatorPenaltyLedger m_validatorPenaltyLedger;
    StakingRegistry m_stakingRegistry;

    // Account-state cache: rebuilt lazily when the chain tip advances.
    mutable std::optional<core::AccountStateView> m_accountStateCache;
    mutable std::uint64_t m_accountStateCacheHeight = 0;
};

enum class NodeRuntimeStartStatus {
    STARTED,
    INVALID_CONFIG,
    GENESIS_BUILD_FAILED
};

std::string nodeRuntimeStartStatusToString(
    NodeRuntimeStartStatus status
);

class NodeRuntimeStartResult {
public:
    NodeRuntimeStartResult();

    static NodeRuntimeStartResult started(
        NodeRuntime runtime
    );

    static NodeRuntimeStartResult rejected(
        NodeRuntimeStartStatus status,
        std::string reason
    );

    NodeRuntimeStartStatus status() const;
    const std::string& reason() const;
    bool started() const;
    const NodeRuntime& runtime() const;

    std::string serialize() const;

private:
    NodeRuntimeStartStatus m_status;
    std::string m_reason;
    NodeRuntime m_runtime;
};

class NodeRuntimeFactory {
public:
    static NodeRuntimeStartResult startFromGenesis(
        const NodeRuntimeConfig& config
    );
};

} // namespace nodo::node

#endif
