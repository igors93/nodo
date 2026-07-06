#include "config/GenesisRegistry.hpp"

#include "crypto/AddressDerivation.hpp"
#include "crypto/KeyPair.hpp"

#include <cassert>
#include <string>

namespace {

using nodo::config::GenesisLookupResult;
using nodo::config::GenesisRegistry;
using nodo::crypto::AddressDerivation;
using nodo::crypto::KeyPair;

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

void testSoakGenesisFound() {
  const GenesisLookupResult result = GenesisRegistry::get("localnet-soak");
  assert(result.found());
  assert(result.genesis().networkParameters().networkName() == "localnet-soak");
  assert(result.genesis().bootstrapValidators().size() == 3);
  assert(result.genesis().genesisAccounts().size() == 1);
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
  assert(GenesisRegistry::hasRegisteredGenesis("localnet-soak"));
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

// A bootstrap validator's own address is BLS-derived (its consensus key),
// but governance votes and exit/unjail requests are mempool transactions,
// verified under an Ed25519-only security context. Without a distinct
// Ed25519 owner, the validator could never cast an authorized vote.
void testLocalnetValidatorHasDistinctVotingOwner() {
  const GenesisLookupResult result = GenesisRegistry::get("localnet");
  assert(result.found());
  assert(result.genesis().bootstrapValidators().size() == 1);

  const auto &bootstrapValidator =
      result.genesis().bootstrapValidators().front();
  const std::string expectedOwner =
      AddressDerivation::deriveFromPublicKey(
          KeyPair::createDeterministicEd25519KeyPair(
              GenesisRegistry::localnetValidatorOwnerKeySeed())
              .publicKey())
          .value();

  assert(bootstrapValidator.effectiveOwnerAddress() == expectedOwner);
  assert(bootstrapValidator.effectiveOwnerAddress() !=
         bootstrapValidator.validatorAddress());
}

void testSoakValidatorsHaveDistinctVotingOwners() {
  const GenesisLookupResult result = GenesisRegistry::get("localnet-soak");
  assert(result.found());
  assert(result.genesis().bootstrapValidators().size() == 3);

  for (std::size_t index = 0; index < 3; ++index) {
    const auto &bootstrapValidator =
        result.genesis().bootstrapValidators()[index];
    const std::string expectedOwner =
        AddressDerivation::deriveFromPublicKey(
            KeyPair::createDeterministicEd25519KeyPair(
                GenesisRegistry::soakValidatorOwnerKeySeed(index))
                .publicKey())
            .value();

    assert(bootstrapValidator.effectiveOwnerAddress() == expectedOwner);
    assert(bootstrapValidator.effectiveOwnerAddress() !=
           bootstrapValidator.validatorAddress());
  }
}

} // namespace

int main() {
  testLocalnetGenesisFound();
  testTestnetCandidateFound();
  testSoakGenesisFound();
  testMainnetMissing();
  testUnknownNetworkMissing();
  testHasRegisteredGenesis();
  testRegisteredGenesisId();
  testGenesisIsDeterministic();
  testLocalnetAndTestnetGenesisAreDifferent();
  testLocalnetValidatorHasDistinctVotingOwner();
  testSoakValidatorsHaveDistinctVotingOwners();
  return 0;
}
