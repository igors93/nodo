#ifndef NODO_CONFIG_NETWORK_PROFILE_REGISTRY_HPP
#define NODO_CONFIG_NETWORK_PROFILE_REGISTRY_HPP

#include "config/NetworkParameters.hpp"

#include <string>
#include <vector>

namespace nodo::config {

/*
 * NetworkProfileRegistry is the authoritative source of known Nodo network
 * profiles and their canonical parameters.
 *
 * Security principle:
 * A node must never accept network parameters from an unknown or untrusted
 * source. All official network parameters are hard-coded here so that an
 * operator cannot accidentally (or maliciously) substitute custom parameters
 * that weaken safety invariants.
 */
class NetworkProfileRegistry {
public:
  static bool isKnown(const std::string &networkName);

  static bool isOfficialNetwork(const std::string &networkName);

  static bool isMainnetLocked(const std::string &networkName);

  static std::vector<std::string> knownProfiles();

  static NetworkParameters get(const std::string &networkName);
};

} // namespace nodo::config

#endif
