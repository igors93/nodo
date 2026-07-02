#include "config/GenesisRegistry.hpp"

#include <cassert>
#include <string>

namespace {

using nodo::config::GenesisLookupResult;
using nodo::config::GenesisRegistry;

void testLocalnetGenesisFound() {
  const GenesisLookupResult result = GenesisRegistry::get("localnet");
  assert(result.found());
  assert(!result.genesis().deterministicId().empty());
  assert(result.genesis().networkParameters().networkName() == "localnet");
  assert(!result.genesis().bootstrapValidators().empty());
  assert(!result.genesis().genesisAccounts().empty());
}

void testTestnetCandidateFound() {
  const GenesisLookupResult result = GenesisRegistry::get("testnet-candidate");
  assert(result.found());
  assert(!result.genesis().deterministicId().empty());
  assert(result.genesis().networkParameters().networkName() ==
         "testnet-candidate");
  assert(!result.genesis().bootstrapValidators().empty());
}

void testMainnetMissing() {
  const GenesisLookupResult result = GenesisRegistry::get("mainnet");
  assert(!result.found());
  assert(!result.reason().empty());
  assert(result.reason().find("mainnet") != std::string::npos);
}

void testUnknownNetworkMissing() {
  const GenesisLookupResult result = GenesisRegistry::get("fantasy-network");
  assert(!result.found());
  assert(!result.reason().empty());
  assert(result.reason().find("fantasy-network") != std::string::npos);
}

void testHasRegisteredGenesis() {
  assert(GenesisRegistry::hasRegisteredGenesis("localnet"));
  assert(GenesisRegistry::hasRegisteredGenesis("testnet-candidate"));
  assert(!GenesisRegistry::hasRegisteredGenesis("mainnet"));
  assert(!GenesisRegistry::hasRegisteredGenesis("unknown"));
}

void testRegisteredGenesisId() {
  const std::string localnetId =
      GenesisRegistry::registeredGenesisId("localnet");
  assert(!localnetId.empty());

  const std::string testnetId =
      GenesisRegistry::registeredGenesisId("testnet-candidate");
  assert(!testnetId.empty());
  assert(localnetId != testnetId);

  const std::string mainnetId = GenesisRegistry::registeredGenesisId("mainnet");
  assert(mainnetId.empty());

  const std::string unknownId = GenesisRegistry::registeredGenesisId("bogus");
  assert(unknownId.empty());
}

void testGenesisIsDeterministic() {
  const GenesisLookupResult first = GenesisRegistry::get("localnet");
  const GenesisLookupResult second = GenesisRegistry::get("localnet");
  assert(first.found() && second.found());
  assert(first.genesis().deterministicId() ==
         second.genesis().deterministicId());
}

void testLocalnetAndTestnetGenesisAreDifferent() {
  const std::string localnetId =
      GenesisRegistry::registeredGenesisId("localnet");
  const std::string testnetId =
      GenesisRegistry::registeredGenesisId("testnet-candidate");
  assert(localnetId != testnetId);
}

} // namespace

int main() {
  testLocalnetGenesisFound();
  testTestnetCandidateFound();
  testMainnetMissing();
  testUnknownNetworkMissing();
  testHasRegisteredGenesis();
  testRegisteredGenesisId();
  testGenesisIsDeterministic();
  testLocalnetAndTestnetGenesisAreDifferent();
  return 0;
}
