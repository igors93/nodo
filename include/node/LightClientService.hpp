#ifndef NODO_NODE_LIGHT_CLIENT_SERVICE_HPP
#define NODO_NODE_LIGHT_CLIENT_SERVICE_HPP

#include "node/LightClientProtocol.hpp"
#include "node/NodeRuntime.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nodo::node {

class LightClientService {
public:
  static std::optional<LightClientHeader> headerAt(const NodeRuntime &runtime,
                                                   std::uint64_t height);

  static std::vector<LightClientHeader> headerRange(const NodeRuntime &runtime,
                                                    std::uint64_t fromHeight,
                                                    std::uint64_t maxHeaders);

  static std::optional<LightClientHeader>
  latestFinalizedHeader(const NodeRuntime &runtime);

  static std::string checkpointJson(const NodeRuntime &runtime);
  static std::string headerRangeJson(const NodeRuntime &runtime,
                                     std::uint64_t fromHeight,
                                     std::uint64_t maxHeaders);
  static std::string accountProofJson(const NodeRuntime &runtime,
                                      const std::string &address);
  static std::string transactionProofJson(const NodeRuntime &runtime,
                                          const std::string &transactionId);
};

} // namespace nodo::node

#endif
