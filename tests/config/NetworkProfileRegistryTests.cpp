#include "config/NetworkProfileRegistry.hpp"

#include <cassert>
#include <stdexcept>
#include <string>

namespace {

void testKnownProfiles() {
  assert(nodo::config::NetworkProfileRegistry::isKnown("localnet"));
  assert(nodo::config::NetworkProfileRegistry::isKnown("testnet-candidate"));
  assert(nodo::config::NetworkProfileRegistry::isKnown("mainnet"));
}

void testUnknownProfile() {
  assert(!nodo::config::NetworkProfileRegistry::isKnown("unknown-network"));
  assert(!nodo::config::NetworkProfileRegistry::isKnown(""));
}

void testOfficialNetworks() {
  assert(nodo::config::NetworkProfileRegistry::isOfficialNetwork(
      "testnet-candidate"));
  assert(nodo::config::NetworkProfileRegistry::isOfficialNetwork("mainnet"));
  assert(!nodo::config::NetworkProfileRegistry::isOfficialNetwork("localnet"));
}

void testMainnetLocked() {
  assert(nodo::config::NetworkProfileRegistry::isMainnetLocked("mainnet"));
  assert(!nodo::config::NetworkProfileRegistry::isMainnetLocked(
      "testnet-candidate"));
  assert(!nodo::config::NetworkProfileRegistry::isMainnetLocked("localnet"));
}

void testGetLocalnet() {
  const auto params = nodo::config::NetworkProfileRegistry::get("localnet");
  assert(params.isValid());
  assert(params.chainId() == "nodo-localnet-1");
}

void testGetTestnetCandidate() {
  const auto params =
      nodo::config::NetworkProfileRegistry::get("testnet-candidate");
  assert(params.isValid());
  assert(params.chainId() == "nodo-testnet-1");
  assert(params.finalityDepth() >= 2);
  assert(params.minimumFeeRawUnits() > 0);
  assert(params.minimumValidatorCount() >= 4);
}

void testGetUnknownThrows() {
  bool threw = false;
  try {
    nodo::config::NetworkProfileRegistry::get("unknown-xyz");
  } catch (const std::invalid_argument &) {
    threw = true;
  }
  assert(threw);
}

void testGetMainnetThrows() {
  bool threw = false;
  try {
    nodo::config::NetworkProfileRegistry::get("mainnet");
  } catch (const std::invalid_argument &) {
    threw = true;
  }
  assert(threw);
}

void testKnownProfilesList() {
  const auto profiles = nodo::config::NetworkProfileRegistry::knownProfiles();
  assert(!profiles.empty());
  assert(profiles.size() >= 3);
}

} // namespace

int main() {
  testKnownProfiles();
  testUnknownProfile();
  testOfficialNetworks();
  testMainnetLocked();
  testGetLocalnet();
  testGetTestnetCandidate();
  testGetUnknownThrows();
  testGetMainnetThrows();
  testKnownProfilesList();
  return 0;
}
