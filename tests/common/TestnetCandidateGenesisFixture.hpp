#ifndef NODO_TESTS_COMMON_TESTNET_CANDIDATE_GENESIS_FIXTURE_HPP
#define NODO_TESTS_COMMON_TESTNET_CANDIDATE_GENESIS_FIXTURE_HPP

// testnet-candidate has no built-in genesis: production code only accepts an
// operator genesis document built from externally generated keys. Tests that
// need a testnet-candidate data directory build one here, from seeds that
// exist only in test code and never in the shipped binary.

#include "config/GenesisDocumentCodec.hpp"
#include "config/NetworkParameters.hpp"
#include "config/NetworkProfileRegistry.hpp"
#include "crypto/AddressDerivation.hpp"
#include "crypto/KeyPair.hpp"
#include "storage/AtomicFile.hpp"
#include "utils/Amount.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace nodo::tests {

inline std::string testnetCandidateTestValidatorSeed(std::size_t index) {
  return "test-only-testnet-candidate-validator-" + std::to_string(index);
}

inline std::string testnetCandidateTestOwnerSeed(std::size_t index) {
  return "test-only-testnet-candidate-owner-" + std::to_string(index);
}

inline std::string testnetCandidateTestUserSeed() {
  return "test-only-testnet-candidate-user";
}

inline std::string testnetCandidateTestAddress(const std::string &seed) {
  return crypto::AddressDerivation::deriveFromPublicKey(
             crypto::KeyPair::createDeterministicEd25519KeyPair(seed)
                 .publicKey())
      .value();
}

inline config::GenesisConfig
testnetCandidateTestGenesis(std::int64_t timestamp = 1900000000) {
  const config::NetworkParameters params =
      config::NetworkProfileRegistry::get("testnet-candidate");

  std::vector<config::BootstrapValidatorConfig> validators;
  for (std::size_t index = 0; index < params.minimumValidatorCount();
       ++index) {
    validators.emplace_back(
        crypto::KeyPair::createDeterministicBls12381KeyPair(
            testnetCandidateTestValidatorSeed(index))
            .publicKey(),
        1, 1, "testnet-candidate-genesis-validator-" + std::to_string(index),
        testnetCandidateTestAddress(testnetCandidateTestOwnerSeed(index)));
  }

  return config::GenesisConfig(
      params, timestamp, std::move(validators),
      {config::GenesisAccountConfig(
          testnetCandidateTestAddress(testnetCandidateTestUserSeed()),
          utils::Amount::fromRawUnits(1000000000000), 0)},
      "nodo-testnet-candidate-test-genesis");
}

// Writes the test genesis document to path and returns path.
inline std::filesystem::path
writeTestnetCandidateTestGenesis(const std::filesystem::path &path) {
  if (path.has_parent_path()) {
    std::filesystem::create_directories(path.parent_path());
  }
  storage::AtomicFile::writeTextFile(
      path, config::GenesisDocumentCodec::encode(testnetCandidateTestGenesis()));
  return path;
}

} // namespace nodo::tests

#endif
