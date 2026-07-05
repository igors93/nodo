#include "node/NodeDaemon.hpp"

#include <iostream>

#include "consensus/BlockFinalizer.hpp"
#include "consensus/BlockProductionPhase.hpp"
#include "consensus/BlockProposalPhase.hpp"
#include "consensus/ProposerSchedule.hpp"
#include "crypto/ProtocolCryptoContext.hpp"
#include "crypto/Signer.hpp"
#include "node/ChainSyncMessages.hpp"
#include "node/FinalizedArtifactGossipAdmission.hpp"
#include "node/FinalizedBlockRecordStore.hpp"
#include "node/NodeDaemon.hpp"
#include "node/PersistentMempoolStore.hpp"
#include "node/RuntimeBlockPipeline.hpp"
#include "node/SignedBlockProposalMessage.hpp"
#include "node/TransactionAdmissionValidator.hpp"
#include "p2p/DiscoveryService.hpp"
#include "p2p/EclipseGuard.hpp"
#include "p2p/NetworkEnvelope.hpp"
#include "p2p/Peer.hpp"

namespace nodo::node {

NodeDaemon::NodeDaemon(NodeDaemonConfig config,
                       const crypto::CryptoPolicy &policy,
                       const crypto::SignatureProvider &provider)
    : m_config(std::move(config)), m_policy(policy), m_provider(provider),
      m_orchestrator(m_config.orchestratorConfig, policy, provider),
      m_seenTxCache(m_config.seenCacheMaxEntries, m_config.seenCacheTtlSeconds),
      m_running(false) {}

NodeDaemon::~NodeDaemon() { stop(); }

NodeOrchestratorStartResult NodeDaemon::start() {
  auto result = m_orchestrator.start();
  if (!result.running())
    return result;

  const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();

  registerStaticPeers(now);
  m_running.store(true);
  return result;
}

void NodeDaemon::runBlocking(std::int64_t tickIntervalMs) {
  while (m_running.load()) {
    const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();

    tick(static_cast<std::int64_t>(now));

    std::this_thread::sleep_for(std::chrono::milliseconds(tickIntervalMs));
  }
}

void NodeDaemon::stop() {
  m_running.store(false);
  m_orchestrator.stop();
}

bool NodeDaemon::isRunning() const { return m_running.load(); }

void NodeDaemon::setLocalSigner(crypto::Signer signer) {
  m_localSigner = signer;
  m_orchestrator.setLocalSigner(std::move(signer));
}

void NodeDaemon::setLocalNodeIdentity(crypto::KeyPair nodeIdentityKey) {
  m_orchestrator.setLocalNodeIdentity(std::move(nodeIdentityKey));
}

void NodeDaemon::tick(std::int64_t now) {
  m_orchestrator.tick(now);
  processTransactionGossip(now);
  processFinalizedArtifacts(now);
  maybeProposeBlock(now);
}

const NodeOrchestrator &NodeDaemon::orchestrator() const {
  return m_orchestrator;
}

void NodeDaemon::registerStaticPeers(std::int64_t now) {
  for (const auto &peer : m_config.staticPeers) {
    if (!peer.isValid())
      continue;

    const p2p::PeerMetadata meta(peer.nodeId,
                                 p2p::PeerEndpoint(peer.host, peer.port), "",
                                 now, now, 0, false);

    m_orchestrator.registerBootstrapPeer(meta, now);
  }
}

void NodeDaemon::processTransactionGossip(std::int64_t now) {
  const auto messages = m_orchestrator.drainGossipInbox(
      p2p::NetworkMessageType::TRANSACTION_GOSSIP);

  for (const auto &envelope : messages) {
    const std::string &payload = envelope.payload();
    if (payload.empty())
      continue;

    // Use payload hash as dedup key: two peers forwarding the same tx
    // have the same payloadHash but different envelope messageIds.
    const std::string &cacheKey = envelope.payloadHash();
    if (cacheKey.empty())
      continue;

    m_seenTxCache.evictExpired(now);

    if (!m_seenTxCache.markSeen(cacheKey, now)) {
      continue; // already processed this tx this window
    }

    const auto decoded = PersistentMempoolStore::deserializeGossip(
        payload, m_policy, crypto::SecurityContext::USER_TRANSACTION,
        m_orchestrator.runtime()
            .config()
            .genesisConfig()
            .networkParameters()
            .chainId());

    bool admitted = false;
    if (decoded.has_value()) {
      NodeRuntime &runtime = m_orchestrator.mutableRuntime();
      const auto &network =
          runtime.config().genesisConfig().networkParameters();
      const crypto::ProtocolCryptoContext cryptoContext =
          crypto::ProtocolCryptoContext::fromNetworkName(network.networkName());
      if (cryptoContext.isValid()) {
        const core::AccountStateView accounts = runtime.cachedAccountStateAtTip(
            static_cast<std::int64_t>(runtime.effectiveMinimumFeeRawUnits()));
        const TransactionAdmissionContext admissionContext(
            accounts, runtime.mempool(), runtime.stakingRegistry(),
            runtime.validatorRegistry(), runtime.governanceExecutor(),
            runtime.blockchain().size());
        const TransactionAdmissionResult validation =
            TransactionAdmissionValidator::validateNetworkSubmission(
                decoded->transaction, network, accounts, runtime.mempool(),
                cryptoContext.policy(),
                crypto::SecurityContext::USER_TRANSACTION,
                cryptoContext.userSignatureProvider(),
                runtime.effectiveMinimumFeeRawUnits(), &admissionContext);
        if (validation.accepted()) {
          auto result = runtime.mutableMempool().admitTransaction(
              decoded->transaction, m_policy,
              crypto::SecurityContext::USER_TRANSACTION, decoded->acceptedAt);
          admitted = result.success();
          if (!admitted) {
            std::cout << "[DEBUG] NodeDaemon processTransactionGossip "
                         "admitTransaction failed: "
                      << result.reason() << std::endl;
          }
        } else {
          std::cout
              << "[NodeDaemon] processTransactionGossip validation rejected: "
              << validation.reason() << std::endl;
        }
      }
    } else {
      std::cout
          << "[NodeDaemon] processTransactionGossip failed to decode payload"
          << std::endl;
    }

    if (admitted) {
      const std::int64_t currentSecond = now / 1000;
      if (m_txRelayBudgetSecond != currentSecond) {
        m_txRelayBudgetSecond = currentSecond;
        m_txRelayBudgetCounter = 0;
      }

      const std::uint32_t relayLimit =
          m_config.orchestratorConfig.genesisConfig()
              .networkParameters()
              .maxTransactionRelayPerSecond();
      if (m_txRelayBudgetCounter < relayLimit) {
        m_txRelayBudgetCounter++;
        m_orchestrator.gossipBroadcast(
            p2p::NetworkMessageType::TRANSACTION_GOSSIP, payload, now);
      } else {
        m_txRelayDroppedCount++;
        std::cout << "[NodeDaemon] Dropping transaction gossip relay to avoid "
                     "amplification (budget exceeded)."
                  << std::endl;
      }
    }
  }
}

void NodeDaemon::processFinalizedArtifacts(std::int64_t now) {
  const auto messages = m_orchestrator.drainGossipInbox(
      p2p::NetworkMessageType::FINALIZED_BLOCK_ARTIFACT);

  FinalizedBlockRecordStore store(
      m_config.orchestratorConfig.dataDirectory().rootPath());

  for (const auto &envelope : messages) {
    const FinalizedArtifactGossipAdmissionResult result =
        FinalizedArtifactGossipAdmission::admit(envelope,
                                                m_orchestrator.mutableRuntime(),
                                                m_policy, m_provider, store);

    std::cout << "[DEBUG] processFinalizedArtifacts received artifact. "
              << "status: " << (int)result.status()
              << ", reason: " << result.reason() << std::endl;

    if (result.fatalConsistencyError()) {
      std::abort();
    }

    if (result.status() ==
        FinalizedArtifactGossipAdmissionStatus::BLOCK_UNAVAILABLE) {
      try {
        const consensus::FinalizedBlockRecord record =
            consensus::FinalizedBlockRecord::deserialize(envelope.payload());
        if (record.isStructurallyValid()) {
          const auto &network = m_orchestrator.runtime()
                                    .config()
                                    .genesisConfig()
                                    .networkParameters();
          const ChainStatusMessage syntheticStatus(
              network.networkName(), network.chainId(),
              m_orchestrator.runtime().config().localPeer().protocolVersion(),
              record.blockIndex(), record.blockHash(), record.blockIndex(),
              record.blockHash());

          std::cout << "[DEBUG] Triggering sync for height "
                    << record.blockIndex() << std::endl;
          m_orchestrator.triggerSyncIfBehind(syntheticStatus,
                                             envelope.senderNodeId(), now);
        } else {
          std::cout
              << "[DEBUG] BLOCK_UNAVAILABLE but record structurally invalid."
              << std::endl;
        }
      } catch (const std::exception &e) {
        std::cout << "[DEBUG] BLOCK_UNAVAILABLE exception: " << e.what()
                  << std::endl;
      }
    }
  }
}

void NodeDaemon::maybeProposeBlock(std::int64_t now) {
  if (!m_localSigner.has_value())
    return;
  if (!m_orchestrator.isRunning())
    return;

  auto &runtime = m_orchestrator.mutableRuntime();
  if (!runtime.isRunning())
    return;

  const std::string chainId =
      m_config.orchestratorConfig.genesisConfig().networkParameters().chainId();
  const auto &state = runtime.consensusRoundManager().currentState();

  const std::uint64_t height = state.height();
  const std::uint64_t round = state.round();

  if (!runtime.validatorSetHistory().hasSet(height))
    return;
  const auto &validators = runtime.validatorSetHistory().setAt(height);

  if (validators.totalConsensusWeight() == 0)
    return;

  const std::string proposer = consensus::ProposerSchedule::selectProposer(
      validators, chainId, height, round);

  if (proposer == m_localSigner->address()) {
    if (m_lastProposedRound.first != height ||
        m_lastProposedRound.second != round) {
      // NodeDaemon may be ticked every 20-50ms, while consensus timestamps are
      // second-granularity. Retrying an unavailable block on every daemon tick
      // performs the same expensive work and floods logs without observing a
      // different protocol time. Retry once when the next second begins.
      if (m_lastProposalAttemptRound.first == height &&
          m_lastProposalAttemptRound.second == round &&
          m_lastProposalAttemptAt == now) {
        return;
      }
      m_lastProposalAttemptRound = {height, round};
      m_lastProposalAttemptAt = now;

      std::optional<core::Block> candidateOpt =
          m_orchestrator.produceBlock(height, round, now);

      if (candidateOpt.has_value()) {
        consensus::BlockProposalResult proposal =
            consensus::BlockProposalPhase::propose(
                *candidateOpt, m_localSigner->address(), round, now,
                *m_localSigner, m_orchestrator.mutableTcpRuntime().gossipMesh(),
                m_provider);

        if (proposal.proposed()) {
          m_lastProposedRound = {height, round};

          // BlockProposalPhase::propose() already broadcasts to every peer.
          // Only inject the proposal locally here; broadcasting it again
          // doubled normal consensus traffic and consumed peer-rate budgets.
          m_orchestrator.gossipInjectLoopback(
              p2p::NetworkMessageType::BLOCK_PROPOSAL,
              proposal.serializedProposal(), now);
        }
      }
    }
  }
}

void NodeDaemon::maintainPeerConnections(std::int64_t now) {
  if (now < m_lastConnectionMaintenanceAt + 5000) {
    return; // Check every 5 seconds
  }
  m_lastConnectionMaintenanceAt = now;

  auto &gossipMesh = m_orchestrator.mutableTcpRuntime().gossipMesh();
  const auto activePeers = gossipMesh.peerRegistry().activePeersAt(now);

  std::vector<p2p::PeerSubnetInfo> subnets;
  for (const auto &peer : activePeers) {
    p2p::PeerSubnetInfo info;
    info.peerId = peer.nodeId();
    info.ipAddress = peer.endpoint().host();
    info.port = peer.endpoint().port();
    info.subnetPrefix =
        p2p::PeerSubnetInfo::extractSubnetPrefix(info.ipAddress);
    subnets.push_back(info);
  }

  p2p::EclipseGuardConfig eclipseConfig =
      gossipMesh.config().eclipseGuardConfig();
  eclipseConfig.maxSingleSubnetFraction = m_config.maxFractionPerSubnet;
  const p2p::EclipseGuard guard(eclipseConfig);

  std::vector<std::string> evictions =
      guard.recommendEvictions(subnets, activePeers.size());
  for (const std::string &evictedNodeId : evictions) {
    std::cout << "[NodeDaemon] Evicting peer " << evictedNodeId
              << " to maintain EclipseGuard subnet limits." << std::endl;
    m_orchestrator.mutableTcpRuntime().transport().disconnect(
        m_config.orchestratorConfig.localPeer().peerId(), evictedNodeId);
  }

  std::size_t outboundConnections =
      m_orchestrator.mutableTcpRuntime().transport().connectedOutboundCount();
  if (outboundConnections < m_config.minOutboundConnections) {
    p2p::DiscoveryService *discovery = m_orchestrator.discoveryService();
    if (discovery != nullptr) {
      std::size_t needed =
          m_config.minOutboundConnections - outboundConnections;
      std::vector<p2p::DiscoveryPeerInfo> candidates =
          discovery->findClosestPeers(
              m_config.orchestratorConfig.localPeer().peerId(), needed * 2);
      std::size_t added = 0;
      for (const auto &candidate : candidates) {
        if (added >= needed)
          break;
        if (candidate.peerId ==
            m_config.orchestratorConfig.localPeer().peerId())
          continue;
        std::cout << "[NodeDaemon] Discovered candidate " << candidate.host
                  << ":" << candidate.tcpPort << ", registering." << std::endl;
        p2p::PeerEndpoint ep(candidate.host, candidate.tcpPort);
        p2p::PeerMetadata meta(candidate.peerId, ep, "", now, now, 0, false);
        m_orchestrator.addAndConnectPeer(meta, now);
        added++;
      }
    }
  }
}

} // namespace nodo::node
