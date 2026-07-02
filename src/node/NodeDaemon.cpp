#include "node/NodeDaemon.hpp"

#include "consensus/BlockFinalizer.hpp"
#include "consensus/BlockProductionPhase.hpp"
#include "consensus/BlockProposalPhase.hpp"
#include "consensus/ProposerSchedule.hpp"
#include "crypto/ProtocolCryptoContext.hpp"
#include "node/ChainSyncMessages.hpp"
#include "node/FinalizedArtifactGossipAdmission.hpp"
#include "node/FinalizedBlockRecordStore.hpp"
#include "node/PersistentMempoolStore.hpp"
#include "node/RuntimeBlockPipeline.hpp"
#include "node/SignedBlockProposalMessage.hpp"
#include "node/TransactionAdmissionValidator.hpp"
#include "p2p/Peer.hpp"

#include <chrono>
#include <cstdlib>
#include <thread>

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
          admitted =
              runtime.mutableMempool()
                  .admitTransaction(decoded->transaction, m_policy,
                                    crypto::SecurityContext::USER_TRANSACTION,
                                    decoded->acceptedAt)
                  .success();
        }
      }
    }

    if (admitted) {
      m_orchestrator.gossipBroadcast(
          p2p::NetworkMessageType::TRANSACTION_GOSSIP, payload, now);
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
    if (result.fatalConsistencyError()) {
      // Finality must never remain memory-only or diverge from its
      // durable proof. Preserve the existing fail-stop safety policy.
      std::abort();
    }

    // A finalized artifact for a height we do not have yet means this node
    // fell behind: either it missed rounds while offline, or the network
    // kept finalizing blocks without ever hitting a round timeout (the only
    // other event that broadcasts CHAIN_STATUS). Without this hook, such a
    // node would keep rejecting every FINALIZED_BLOCK_ARTIFACT it receives
    // as BLOCK_UNAVAILABLE forever, staying stuck waiting for a proposal at
    // its stale height instead of catching up. Treat the artifact itself as
    // sync-trigger evidence, exactly like an ahead CHAIN_STATUS message.
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
          m_orchestrator.triggerSyncIfBehind(syntheticStatus,
                                             envelope.senderNodeId(), now);
        }
      } catch (const std::exception &) {
        // Malformed payload here was already reported by admit(); do not
        // let a second parse failure interrupt the tick loop.
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
      m_lastProposedRound = {height, round};

      std::optional<core::Block> candidateOpt =
          m_orchestrator.produceBlock(height, round, now);

      if (candidateOpt.has_value()) {
        consensus::BlockProposalResult proposal =
            consensus::BlockProposalPhase::propose(
                *candidateOpt, m_localSigner->address(), round, now,
                *m_localSigner, m_orchestrator.mutableTcpRuntime().gossipMesh(),
                m_provider);

        if (proposal.proposed()) {
          m_orchestrator.gossipBroadcast(
              p2p::NetworkMessageType::BLOCK_PROPOSAL,
              proposal.serializedProposal(), now);

          m_orchestrator.gossipInjectLoopback(
              p2p::NetworkMessageType::BLOCK_PROPOSAL,
              proposal.serializedProposal(), now);
        }
      }
    }
  }
}

} // namespace nodo::node
