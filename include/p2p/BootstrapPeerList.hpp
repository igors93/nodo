#ifndef NODO_P2P_BOOTSTRAP_PEER_LIST_HPP
#define NODO_P2P_BOOTSTRAP_PEER_LIST_HPP

#include "p2p/Peer.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace nodo::p2p {

/*
 * BootstrapPeerList manages and validates the set of initial peers used to
 * join a network on first startup.
 *
 * Security principle:
 * Bootstrap peers are the first trust point for a new node. A list with
 * invalid or empty entries must be rejected rather than silently ignored.
 * Nodes must never accept a bootstrap list with zero valid entries on an
 * official network.
 */
class BootstrapPeerList {
public:
  static bool isValidPeer(const PeerEndpoint &endpoint);

  static bool validateAll(const std::vector<PeerEndpoint> &peers,
                          std::string &reason);

  static std::vector<PeerEndpoint>
  parseFromLines(const std::vector<std::string> &lines);

  static std::vector<PeerEndpoint>
  loadFromFile(const std::filesystem::path &path, std::string &reason);
};

} // namespace nodo::p2p

#endif
