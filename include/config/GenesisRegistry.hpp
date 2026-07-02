#ifndef NODO_CONFIG_GENESIS_REGISTRY_HPP
#define NODO_CONFIG_GENESIS_REGISTRY_HPP

#include "config/NetworkParameters.hpp"

#include <cstddef>
#include <string>

namespace nodo::config {

/*
 * GenesisLookupResult carries the outcome of a GenesisRegistry lookup.
 *
 * Security principle:
 * A missing or unknown genesis is not a fallback — it is a hard rejection.
 * Any network that can reach runtime startup, reload, audit, or readiness
 * must have an explicit registered genesis. Placeholder behavior is forbidden.
 */
class GenesisLookupResult {
public:
  GenesisLookupResult();

  static GenesisLookupResult found(GenesisConfig genesis);
  static GenesisLookupResult missing(std::string reason);

  bool found() const;
  const GenesisConfig &genesis() const;
  const std::string &reason() const;

private:
  bool m_found;
  GenesisConfig m_genesis;
  std::string m_reason;
};

/*
 * GenesisRegistry is the single authority for registered genesis
 * configurations.
 *
 * Every network that can start a runtime must have a registered genesis here.
 * Unknown networks and networks without a registered genesis fail immediately.
 * This registry must never silently substitute a placeholder genesis.
 *
 * Development keys embedded in localnet, localnet-soak and
 * testnet-candidate genesis are deterministic and clearly labeled. They must
 * not be used in production networks.
 */
class GenesisRegistry {
public:
  // Returns the registered genesis for the given network name.
  // Returns missing() if the network is unknown or has no registered genesis.
  static GenesisLookupResult get(const std::string &networkName);

  // Returns true if a registered genesis exists for the given network.
  static bool hasRegisteredGenesis(const std::string &networkName);

  // Returns the deterministic genesis ID for a network without constructing
  // the full genesis. Returns empty string if not registered.
  static std::string registeredGenesisId(const std::string &networkName);

  // Deterministic seed used to derive the localnet user account keypair.
  // Exposed so callers can locate or create the matching key without
  // duplicating the literal string.
  static std::string localnetUserKeySeed();
  static std::string soakUserKeySeed();
  static std::string soakValidatorKeySeed(std::size_t index);
};

} // namespace nodo::config

#endif
