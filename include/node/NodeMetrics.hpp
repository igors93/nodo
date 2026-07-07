#ifndef NODO_NODE_NODE_METRICS_HPP
#define NODO_NODE_NODE_METRICS_HPP

#include "node/NodeEventBus.hpp"
#include "node/NodeRuntime.hpp"
#include "node/SyncHealth.hpp"

#include <cstdint>
#include <string>

namespace nodo::node {

/*
 * NodeMetricsSnapshot is a point-in-time, read-only operational view of a
 * running node. It deliberately contains primitive values only so it can be
 * safely serialized, exported and tested without exposing mutable runtime
 * internals to RPC, health checks or future dashboards.
 */
class NodeMetricsSnapshot {
public:
  std::int64_t collectedAt{0};

  std::string networkName;
  std::string chainId;
  std::string genesisConfigId;
  std::string localPeerId;
  std::string runtimeStatus;

  std::uint64_t height{0};
  std::uint64_t finalizedHeight{0};
  std::uint64_t finalityLag{0};
  std::string latestHash;
  std::string latestStateRoot;

  std::size_t peerCount{0};
  std::size_t validatorCount{0};
  std::size_t mempoolSize{0};
  std::size_t blockCandidateTransactionCount{0};

  std::size_t governanceActiveProposalCount{0};
  std::size_t governanceApprovedProposalCount{0};
  std::size_t governanceExecutableProposalCount{0};
  std::size_t governanceExecutedProposalCount{0};

  std::uint64_t eventLatestSequence{0};
  std::size_t eventRetainedCount{0};
  std::size_t eventMaxRetainedCount{0};

  std::string syncStatus{"UNKNOWN"};
  bool syncHealthy{true};
  std::uint64_t syncTotalSynced{0};
  std::uint64_t syncTotalFailures{0};
  std::int64_t syncLastFailureAt{0};
  std::string syncLastFailureReason;

  bool rpcRunning{false};
  std::string rpcStartError;

  bool runtimeValid{false};
  bool runtimeRunning{false};
  bool runtimeHalted{false};

  bool isValid() const;
  std::string serializeJson() const;
};

class NodeMetricsCollector {
public:
  static NodeMetricsSnapshot collect(const NodeRuntime &runtime,
                                     const SyncHealth *syncHealth,
                                     const NodeEventBus *eventBus,
                                     bool rpcRunning, std::string rpcStartError,
                                     std::int64_t now);
};

} // namespace nodo::node

#endif
