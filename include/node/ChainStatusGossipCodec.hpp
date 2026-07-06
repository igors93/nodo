#ifndef NODO_NODE_CHAIN_STATUS_GOSSIP_CODEC_HPP
#define NODO_NODE_CHAIN_STATUS_GOSSIP_CODEC_HPP

#include "node/ChainSyncMessages.hpp"

#include <optional>
#include <string>

namespace nodo::node {

class ChainStatusGossipCodec {
public:
  static std::string encode(const ChainStatusMessage &status);

  static std::optional<ChainStatusMessage> decode(const std::string &payload);
};

} // namespace nodo::node

#endif
