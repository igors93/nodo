#include "node/NodeRuntime.hpp"

#include "consensus/ProposerSchedule.hpp"
#include "core/GenesisVerifier.hpp"
#include "crypto/ProtocolCryptoContext.hpp"
#include "node/MonetaryFirewall.hpp"
#include "node/ProtocolInvariantChecker.hpp"

#include <sstream>
#include <utility>

namespace nodo::node {

namespace {

void initializeConsensusRound(
    consensus::ConsensusRoundManager& manager,
    const config::GenesisConfig& genesisConfig,
    const core::Blockchain& blockchain,
    const core::ValidatorRegistry& validatorRegistry
) {
    const std::uint64_t nextHeight =
        blockchain.latestBlock().index() + 1;

    constexpr std::uint64_t initialRound = 1;

    const std::string proposer =
        consensus::ProposerSchedule::selectProposer(
            validatorRegistry,
            genesisConfig.networkParameters().chainId(),
            nextHeight,
            initialRound
        );

    manager.advanceToHeight(
        nextHeight,
        initialRound,
        proposer,
        genesisConfig.genesisTimestamp(),
        genesisConfig.networkParameters().targetBlockTimeSeconds()
    );
}

} // namespace

std::string peerUpdateStatusToString(
    PeerUpdateStatus status
) {
    switch (status) {
        case PeerUpdateStatus::ACCEPTED:
            return "ACCEPTED";
        case PeerUpdateStatus::UPDATED:
            return "UPDATED";
        case PeerUpdateStatus::DUPLICATE:
            return "DUPLICATE";
        case PeerUpdateStatus::REJECTED:
            return "REJECTED";
        case PeerUpdateStatus::CAPACITY_REACHED:
            return "CAPACITY_REACHED";
        default:
            return "REJECTED";
    }
}

PeerUpdateResult::PeerUpdateResult()
    : m_status(PeerUpdateStatus::REJECTED),
      m_reason("Uninitialized peer update result."),
      m_peer() {}

PeerUpdateResult PeerUpdateResult::accepted(
    p2p::PeerInfo peer
) {
    PeerUpdateResult result;
    result.m_status = PeerUpdateStatus::ACCEPTED;
    result.m_reason = "";
    result.m_peer = std::move(peer);
    return result;
}

PeerUpdateResult PeerUpdateResult::updated(
    p2p::PeerInfo peer
) {
    PeerUpdateResult result;
    result.m_status = PeerUpdateStatus::UPDATED;
    result.m_reason = "";
    result.m_peer = std::move(peer);
    return result;
}

PeerUpdateResult PeerUpdateResult::duplicate(
    p2p::PeerInfo peer
) {
    PeerUpdateResult result;
    result.m_status = PeerUpdateStatus::DUPLICATE;
    result.m_reason = "Peer already exists with the same metadata.";
    result.m_peer = std::move(peer);
    return result;
}

PeerUpdateResult PeerUpdateResult::rejected(
    PeerUpdateStatus status,
    std::string reason
) {
    PeerUpdateResult result;
    result.m_status = status;
    result.m_reason = std::move(reason);
    return result;
}

PeerUpdateStatus PeerUpdateResult::status() const {
    return m_status;
}

const std::string& PeerUpdateResult::reason() const {
    return m_reason;
}

const p2p::PeerInfo& PeerUpdateResult::peer() const {
    return m_peer;
}

bool PeerUpdateResult::success() const {
    return m_status == PeerUpdateStatus::ACCEPTED ||
           m_status == PeerUpdateStatus::UPDATED ||
           m_status == PeerUpdateStatus::DUPLICATE;
}

std::string PeerUpdateResult::serialize() const {
    std::ostringstream oss;

    oss << "PeerUpdateResult{"
        << "status=" << peerUpdateStatusToString(m_status)
        << ";reason=" << m_reason
        << ";peer=" << (m_peer.isValid() ? m_peer.serialize() : "NONE")
        << "}";

    return oss.str();
}

LocalPeerManager::LocalPeerManager(
    std::size_t maxPeers
)
    : m_maxPeers(maxPeers),
      m_peersById() {}

PeerUpdateResult LocalPeerManager::addOrUpdatePeer(
    const p2p::PeerInfo& peer
) {
    if (m_maxPeers == 0) {
        return PeerUpdateResult::rejected(
            PeerUpdateStatus::REJECTED,
            "Local peer manager maxPeers is zero."
        );
    }

    if (!peer.isValid()) {
        return PeerUpdateResult::rejected(
            PeerUpdateStatus::REJECTED,
            "Peer metadata is invalid."
        );
    }

    const auto existing =
        m_peersById.find(peer.peerId());

    if (existing != m_peersById.end()) {
        if (existing->second.serialize() == peer.serialize()) {
            return PeerUpdateResult::duplicate(existing->second);
        }

        existing->second = peer;
        return PeerUpdateResult::updated(peer);
    }

    if (m_peersById.size() >= m_maxPeers) {
        return PeerUpdateResult::rejected(
            PeerUpdateStatus::CAPACITY_REACHED,
            "Peer manager capacity reached."
        );
    }

    m_peersById.emplace(peer.peerId(), peer);
    return PeerUpdateResult::accepted(peer);
}

bool LocalPeerManager::removePeer(
    const std::string& peerId
) {
    return m_peersById.erase(peerId) > 0;
}

bool LocalPeerManager::hasPeer(
    const std::string& peerId
) const {
    return m_peersById.find(peerId) != m_peersById.end();
}

const p2p::PeerInfo* LocalPeerManager::peer(
    const std::string& peerId
) const {
    const auto found =
        m_peersById.find(peerId);

    if (found == m_peersById.end()) {
        return nullptr;
    }

    return &found->second;
}

const p2p::PeerInfo* LocalPeerManager::bestSyncPeer(
    std::uint64_t localHeight
) const {
    const p2p::PeerInfo* bestPeer = nullptr;

    for (const auto& [_, peerInfo] : m_peersById) {
        if (!peerInfo.isValid() ||
            peerInfo.latestKnownHeight() <= localHeight) {
            continue;
        }

        if (bestPeer == nullptr ||
            peerInfo.latestKnownHeight() > bestPeer->latestKnownHeight() ||
            (peerInfo.latestKnownHeight() == bestPeer->latestKnownHeight() &&
             peerInfo.peerId() < bestPeer->peerId())) {
            bestPeer = &peerInfo;
        }
    }

    return bestPeer;
}

std::vector<p2p::PeerInfo> LocalPeerManager::peers() const {
    std::vector<p2p::PeerInfo> result;
    result.reserve(m_peersById.size());

    for (const auto& [_, peerInfo] : m_peersById) {
        result.push_back(peerInfo);
    }

    return result;
}

std::size_t LocalPeerManager::size() const {
    return m_peersById.size();
}

std::size_t LocalPeerManager::maxPeers() const {
    return m_maxPeers;
}

bool LocalPeerManager::isValid() const {
    if (m_maxPeers == 0 ||
        m_peersById.size() > m_maxPeers) {
        return false;
    }

    for (const auto& [peerId, peerInfo] : m_peersById) {
        if (peerId != peerInfo.peerId() ||
            !peerInfo.isValid()) {
            return false;
        }
    }

    return true;
}

std::string LocalPeerManager::serialize() const {
    std::ostringstream oss;

    oss << "LocalPeerManager{"
        << "maxPeers=" << m_maxPeers
        << ";size=" << m_peersById.size()
        << ";peers=[";

    bool first = true;

    for (const auto& [_, peerInfo] : m_peersById) {
        if (!first) {
            oss << ",";
        }

        oss << peerInfo.serialize();
        first = false;
    }

    oss << "]}";

    return oss.str();
}

std::string nodeRuntimeStatusToString(
    NodeRuntimeStatus status
) {
    switch (status) {
        case NodeRuntimeStatus::RUNNING:
            return "RUNNING";
        case NodeRuntimeStatus::HALTED:
            return "HALTED";
        case NodeRuntimeStatus::STOPPED:
        default:
            return "STOPPED";
    }
}

NodeRuntimeConfig::NodeRuntimeConfig()
    : m_genesisConfig(),
      m_localPeer(),
      m_maxPeers(0) {}

NodeRuntimeConfig::NodeRuntimeConfig(
    config::GenesisConfig genesisConfig,
    p2p::PeerInfo localPeer,
    std::size_t maxPeers
)
    : m_genesisConfig(std::move(genesisConfig)),
      m_localPeer(std::move(localPeer)),
      m_maxPeers(maxPeers) {}

const config::GenesisConfig& NodeRuntimeConfig::genesisConfig() const {
    return m_genesisConfig;
}

const p2p::PeerInfo& NodeRuntimeConfig::localPeer() const {
    return m_localPeer;
}

std::size_t NodeRuntimeConfig::maxPeers() const {
    return m_maxPeers;
}

bool NodeRuntimeConfig::isValid() const {
    return m_genesisConfig.isValid() &&
           m_localPeer.isValid() &&
           m_maxPeers > 0 &&
           m_maxPeers <= m_genesisConfig.networkParameters().maxPeerCount();
}

std::string NodeRuntimeConfig::serialize() const {
    std::ostringstream oss;

    oss << "NodeRuntimeConfig{"
        << "localPeer=" << (m_localPeer.isValid() ? m_localPeer.serialize() : "INVALID")
        << ";maxPeers=" << m_maxPeers
        << ";genesisConfigId=" << m_genesisConfig.deterministicId()
        << "}";

    return oss.str();
}

NodeRuntime::NodeRuntime()
    : m_config(),
      m_status(NodeRuntimeStatus::STOPPED),
      m_blockchain(),
      m_validatorRegistry(),
      m_finalizationRegistry(),
      m_consensusRoundManager(),
      m_mempool(),
      m_peerManager(1) {}

NodeRuntime::NodeRuntime(
    NodeRuntimeConfig config,
    core::Blockchain blockchain,
    core::ValidatorRegistry validatorRegistry
)
    : m_config(std::move(config)),
      m_status(NodeRuntimeStatus::RUNNING),
      m_blockchain(std::move(blockchain)),
      m_validatorRegistry(std::move(validatorRegistry)),
      m_finalizationRegistry(),
      m_consensusRoundManager(),
      m_mempool(),
      m_peerManager(m_config.maxPeers()) {
    if (m_config.genesisConfig().isValid() &&
        !m_blockchain.empty() &&
        m_validatorRegistry.isValid()) {
        initializeConsensusRound(
            m_consensusRoundManager,
            m_config.genesisConfig(),
            m_blockchain,
            m_validatorRegistry
        );
    }
}

const NodeRuntimeConfig& NodeRuntime::config() const {
    return m_config;
}

NodeRuntimeStatus NodeRuntime::status() const {
    return m_status;
}

const core::Blockchain& NodeRuntime::blockchain() const {
    return m_blockchain;
}

core::Blockchain& NodeRuntime::mutableBlockchain() {
    return m_blockchain;
}

const core::ValidatorRegistry& NodeRuntime::validatorRegistry() const {
    return m_validatorRegistry;
}

core::ValidatorRegistry& NodeRuntime::mutableValidatorRegistry() {
    return m_validatorRegistry;
}

const consensus::BlockFinalizationRegistry& NodeRuntime::finalizationRegistry() const {
    return m_finalizationRegistry;
}

consensus::BlockFinalizationRegistry& NodeRuntime::mutableFinalizationRegistry() {
    return m_finalizationRegistry;
}

const consensus::ConsensusRoundManager& NodeRuntime::consensusRoundManager() const {
    return m_consensusRoundManager;
}

consensus::ConsensusRoundManager& NodeRuntime::mutableConsensusRoundManager() {
    return m_consensusRoundManager;
}

const mempool::Mempool& NodeRuntime::mempool() const {
    return m_mempool;
}

mempool::Mempool& NodeRuntime::mutableMempool() {
    return m_mempool;
}

const LocalPeerManager& NodeRuntime::peerManager() const {
    return m_peerManager;
}

const RuntimeSupplyState& NodeRuntime::supplyState() const {
    return m_supplyState;
}

RuntimeSupplyState& NodeRuntime::mutableSupplyState() {
    return m_supplyState;
}

const core::StatePruner& NodeRuntime::statePruner() const {
    return m_statePruner;
}

core::StatePruner& NodeRuntime::mutableStatePruner() {
    return m_statePruner;
}

LocalPeerManager& NodeRuntime::mutablePeerManager() {
    return m_peerManager;
}

bool NodeRuntime::isRunning() const {
    return m_status == NodeRuntimeStatus::RUNNING;
}

bool NodeRuntime::isHalted() const {
    return m_status == NodeRuntimeStatus::HALTED;
}

bool NodeRuntime::isValid() const {
    return isRunning() &&
           m_config.isValid() &&
           !m_blockchain.empty() &&
           m_blockchain.isValid() &&
           m_validatorRegistry.isValid() &&
           m_finalizationRegistry.isValid() &&
           m_consensusRoundManager.currentState().isValid() &&
           m_peerManager.isValid();
}

void NodeRuntime::halt() {
    m_status = NodeRuntimeStatus::HALTED;
}

consensus::ChainForkSummary NodeRuntime::chainSummary() const {
    return consensus::ForkChoicePolicy::summarizeChain(
        m_blockchain,
        m_finalizationRegistry
    );
}

consensus::VoteCollectResult NodeRuntime::submitConsensusVote(
    const consensus::ValidatorVoteRecord& vote
) {
    const crypto::ProtocolCryptoContext cryptoContext =
        crypto::ProtocolCryptoContext::fromNetworkName(
            m_config.genesisConfig().networkParameters().networkName()
        );
    return m_consensusRoundManager.submitVote(
        vote,
        cryptoContext.policy(),
        cryptoContext.validatorSignatureProvider()
    );
}

bool NodeRuntime::advanceConsensusRoundIfTimedOut(
    std::int64_t now
) {
    if (!m_consensusRoundManager.isTimeoutExpired(now)) {
        return false;
    }

    const std::uint64_t newRound =
        m_consensusRoundManager.currentState().round() + 1;

    const std::string proposer =
        consensus::ProposerSchedule::selectProposer(
            m_validatorRegistry,
            m_config.genesisConfig().networkParameters().chainId(),
            m_consensusRoundManager.currentState().height(),
            newRound
        );

    m_consensusRoundManager.advanceRound(
        newRound,
        proposer,
        now,
        m_config.genesisConfig().networkParameters().targetBlockTimeSeconds()
    );

    return true;
}

p2p::PeerMessage NodeRuntime::localChainSummaryMessage(
    const std::string& toPeerId,
    std::int64_t createdAt
) const {
    return p2p::PeerMessageFactory::chainSummary(
        m_config.localPeer(),
        toPeerId,
        chainSummary(),
        createdAt
    );
}

p2p::LocalSyncPlan NodeRuntime::evaluatePeerForSync(
    const p2p::PeerInfo& remotePeer,
    const consensus::ChainForkSummary& remoteSummary,
    std::int64_t createdAt
) const {
    return p2p::LocalNodeSynchronizer::evaluatePeerSummary(
        m_config.localPeer(),
        remotePeer,
        m_blockchain,
        m_finalizationRegistry,
        remoteSummary,
        createdAt
    );
}

std::string NodeRuntime::serialize() const {
    std::ostringstream oss;

    oss << "NodeRuntime{"
        << "status=" << nodeRuntimeStatusToString(m_status)
        << ";config=" << m_config.serialize()
        << ";blockchainSize=" << m_blockchain.size()
        << ";validatorRegistrySize=" << m_validatorRegistry.size()
        << ";finalizationRegistrySize=" << m_finalizationRegistry.size()
        << ";consensusRound=" << m_consensusRoundManager.currentState().serialize()
        << ";peerManager=" << m_peerManager.serialize()
        << "}";

    return oss.str();
}

std::string nodeRuntimeStartStatusToString(
    NodeRuntimeStartStatus status
) {
    switch (status) {
        case NodeRuntimeStartStatus::STARTED:
            return "STARTED";
        case NodeRuntimeStartStatus::INVALID_CONFIG:
            return "INVALID_CONFIG";
        case NodeRuntimeStartStatus::GENESIS_BUILD_FAILED:
            return "GENESIS_BUILD_FAILED";
        default:
            return "INVALID_CONFIG";
    }
}

NodeRuntimeStartResult::NodeRuntimeStartResult()
    : m_status(NodeRuntimeStartStatus::INVALID_CONFIG),
      m_reason("Uninitialized node runtime start result."),
      m_runtime() {}

NodeRuntimeStartResult NodeRuntimeStartResult::started(
    NodeRuntime runtime
) {
    NodeRuntimeStartResult result;
    result.m_status = NodeRuntimeStartStatus::STARTED;
    result.m_reason = "";
    result.m_runtime = std::move(runtime);
    return result;
}

NodeRuntimeStartResult NodeRuntimeStartResult::rejected(
    NodeRuntimeStartStatus status,
    std::string reason
) {
    NodeRuntimeStartResult result;
    result.m_status = status;
    result.m_reason = std::move(reason);
    return result;
}

NodeRuntimeStartStatus NodeRuntimeStartResult::status() const {
    return m_status;
}

const std::string& NodeRuntimeStartResult::reason() const {
    return m_reason;
}

bool NodeRuntimeStartResult::started() const {
    return m_status == NodeRuntimeStartStatus::STARTED &&
           m_runtime.isValid();
}

const NodeRuntime& NodeRuntimeStartResult::runtime() const {
    return m_runtime;
}

std::string NodeRuntimeStartResult::serialize() const {
    std::ostringstream oss;

    oss << "NodeRuntimeStartResult{"
        << "status=" << nodeRuntimeStartStatusToString(m_status)
        << ";reason=" << m_reason
        << ";runtime=" << (m_runtime.isValid() ? m_runtime.serialize() : "NONE")
        << "}";

    return oss.str();
}

NodeRuntimeStartResult NodeRuntimeFactory::startFromGenesis(
    const NodeRuntimeConfig& config
) {
    const core::GenesisVerificationResult genesisVerification =
        core::GenesisVerifier::verify(config.genesisConfig());

    if (!genesisVerification.isValid()) {
        return NodeRuntimeStartResult::rejected(
            NodeRuntimeStartStatus::INVALID_CONFIG,
            "Genesis verification failed: " +
                core::genesisVerificationStatusToString(
                    genesisVerification.status()
                ) +
                ": " +
                genesisVerification.reason()
        );
    }

    if (!config.isValid()) {
        return NodeRuntimeStartResult::rejected(
            NodeRuntimeStartStatus::INVALID_CONFIG,
            "Node runtime config is invalid."
        );
    }

    const config::GenesisBuildResult genesis =
        config::GenesisBuilder::build(
            config.genesisConfig()
        );

    if (!genesis.built()) {
        return NodeRuntimeStartResult::rejected(
            NodeRuntimeStartStatus::GENESIS_BUILD_FAILED,
            genesis.reason()
        );
    }

    NodeRuntime runtime(
        config,
        genesis.blockchain(),
        genesis.validatorRegistry()
    );

    // Initialize the supply state with genesis supply.
    try {
        const utils::Amount genesisSupply =
            MonetaryFirewall::genesisSupply(config.genesisConfig());
        runtime.mutableSupplyState() = RuntimeSupplyState(genesisSupply);
    } catch (const std::exception&) {
        // Genesis supply unavailable — leave supply state at zero.
        // RuntimeMonetaryValidation will handle unavailability gracefully.
    }

    if (!runtime.isValid()) {
        return NodeRuntimeStartResult::rejected(
            NodeRuntimeStartStatus::GENESIS_BUILD_FAILED,
            "Started runtime failed audit."
        );
    }

    const ProtocolInvariantCheckResult invariantCheck =
        ProtocolInvariantChecker::checkRuntime(runtime);

    if (!invariantCheck.passed()) {
        return NodeRuntimeStartResult::rejected(
            NodeRuntimeStartStatus::GENESIS_BUILD_FAILED,
            "Started runtime failed protocol invariant audit: " +
                invariantCheck.reason()
        );
    }

    return NodeRuntimeStartResult::started(runtime);
}

} // namespace nodo::node
