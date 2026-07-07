#include "node/NodeMetrics.hpp"

#include <algorithm>
#include <sstream>
#include <utility>

namespace nodo::node {
namespace {

std::string jsonString(const std::string &value) {
  std::string out;
  out.reserve(value.size() + 2);
  out.push_back('"');
  for (const char c : value) {
    switch (c) {
    case '"':
      out += "\\\"";
      break;
    case '\\':
      out += "\\\\";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      out.push_back(c);
      break;
    }
  }
  out.push_back('"');
  return out;
}

} // namespace

bool NodeMetricsSnapshot::isValid() const {
  return collectedAt >= 0 && !networkName.empty() && !chainId.empty() &&
         !genesisConfigId.empty() && !localPeerId.empty() &&
         !runtimeStatus.empty() && finalizedHeight <= height &&
         finalityLag == height - finalizedHeight;
}

std::string NodeMetricsSnapshot::serializeJson() const {
  std::ostringstream oss;
  oss << "{"
      << "\"collectedAt\":" << collectedAt
      << ",\"networkName\":" << jsonString(networkName)
      << ",\"chainId\":" << jsonString(chainId)
      << ",\"genesisConfigId\":" << jsonString(genesisConfigId)
      << ",\"localPeerId\":" << jsonString(localPeerId)
      << ",\"runtimeStatus\":" << jsonString(runtimeStatus)
      << ",\"height\":" << height << ",\"finalizedHeight\":" << finalizedHeight
      << ",\"finalityLag\":" << finalityLag
      << ",\"latestHash\":" << jsonString(latestHash)
      << ",\"latestStateRoot\":" << jsonString(latestStateRoot)
      << ",\"peerCount\":" << peerCount
      << ",\"validatorCount\":" << validatorCount
      << ",\"mempoolSize\":" << mempoolSize
      << ",\"blockCandidateTransactionCount\":"
      << blockCandidateTransactionCount << ",\"governance\":{"
      << "\"activeProposalCount\":" << governanceActiveProposalCount
      << ",\"approvedProposalCount\":" << governanceApprovedProposalCount
      << ",\"executableProposalCount\":" << governanceExecutableProposalCount
      << ",\"executedProposalCount\":" << governanceExecutedProposalCount << "}"
      << ",\"events\":{"
      << "\"latestSequence\":" << eventLatestSequence
      << ",\"retainedCount\":" << eventRetainedCount
      << ",\"maxRetainedCount\":" << eventMaxRetainedCount << "}"
      << ",\"sync\":{"
      << "\"status\":" << jsonString(syncStatus)
      << ",\"healthy\":" << (syncHealthy ? "true" : "false")
      << ",\"totalSynced\":" << syncTotalSynced
      << ",\"totalFailures\":" << syncTotalFailures
      << ",\"lastFailureAt\":" << syncLastFailureAt
      << ",\"lastFailureReason\":" << jsonString(syncLastFailureReason) << "}"
      << ",\"rpc\":{"
      << "\"running\":" << (rpcRunning ? "true" : "false")
      << ",\"startError\":" << jsonString(rpcStartError) << "}"
      << ",\"runtime\":{"
      << "\"valid\":" << (runtimeValid ? "true" : "false")
      << ",\"running\":" << (runtimeRunning ? "true" : "false")
      << ",\"halted\":" << (runtimeHalted ? "true" : "false") << "}"
      << "}";
  return oss.str();
}

NodeMetricsSnapshot NodeMetricsCollector::collect(const NodeRuntime &runtime,
                                                  const SyncHealth *syncHealth,
                                                  const NodeEventBus *eventBus,
                                                  bool rpcRunning,
                                                  std::string rpcStartError,
                                                  std::int64_t now) {
  NodeMetricsSnapshot snapshot;
  snapshot.collectedAt = now;

  const config::GenesisConfig &genesis = runtime.config().genesisConfig();
  const config::NetworkParameters &network = genesis.networkParameters();

  snapshot.networkName = network.networkName();
  snapshot.chainId = network.chainId();
  snapshot.genesisConfigId = genesis.deterministicId();
  snapshot.localPeerId = runtime.config().localPeer().peerId();
  snapshot.runtimeStatus = nodeRuntimeStatusToString(runtime.status());

  if (!runtime.blockchain().empty()) {
    const core::Block &tip = runtime.blockchain().latestBlock();
    snapshot.height = tip.index();
    snapshot.latestHash = tip.hash();
    snapshot.latestStateRoot =
        tip.hasCanonicalStateRoot() ? tip.stateRoot() : "";
  }

  snapshot.finalizedHeight = std::min<std::uint64_t>(
      runtime.finalizationRegistry().highestFinalizedHeight(), snapshot.height);
  snapshot.finalityLag = snapshot.height - snapshot.finalizedHeight;

  snapshot.peerCount = runtime.peerManager().size();
  snapshot.validatorCount = runtime.validatorRegistry().activeCount();
  snapshot.mempoolSize = runtime.mempool().size();
  snapshot.blockCandidateTransactionCount =
      runtime.mempool().transactionsForBlock(20).size();

  const GovernanceExecutor &governance = runtime.governanceExecutor();
  const std::uint64_t nextHeight = runtime.blockchain().size();
  snapshot.governanceActiveProposalCount = governance.activeProposalCount();
  snapshot.governanceApprovedProposalCount = governance.approvedProposalCount();
  snapshot.governanceExecutableProposalCount =
      governance.executableProposalCount(nextHeight);
  snapshot.governanceExecutedProposalCount = governance.executedProposalCount();

  if (eventBus != nullptr) {
    snapshot.eventLatestSequence = eventBus->latestSequence();
    snapshot.eventRetainedCount = eventBus->retainedCount();
    snapshot.eventMaxRetainedCount = eventBus->maxRetainedEvents();
  }

  if (syncHealth != nullptr) {
    snapshot.syncStatus = syncHealthStatusToString(syncHealth->status());
    snapshot.syncHealthy = syncHealth->isHealthy();
    snapshot.syncTotalSynced = syncHealth->totalSynced();
    snapshot.syncTotalFailures = syncHealth->totalFailures();
    snapshot.syncLastFailureAt = syncHealth->lastFailureAt();
    snapshot.syncLastFailureReason = syncHealth->lastFailureReason();
  }

  snapshot.rpcRunning = rpcRunning;
  snapshot.rpcStartError = std::move(rpcStartError);
  snapshot.runtimeValid = runtime.isValid();
  snapshot.runtimeRunning = runtime.isRunning();
  snapshot.runtimeHalted = runtime.isHalted();

  return snapshot;
}

} // namespace nodo::node
