#include "config/NetworkParameters.hpp"
#include "crypto/KeyPair.hpp"
#include "crypto/PublicKey.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using nodo::config::BootstrapValidatorConfig;
using nodo::config::GenesisAccountConfig;
using nodo::config::GenesisBuilder;
using nodo::config::GenesisBuildStatus;
using nodo::config::GenesisConfig;
using nodo::config::NetworkParameters;
using nodo::crypto::KeyPair;
using nodo::crypto::PublicKey;
using nodo::utils::Amount;

constexpr std::int64_t kTimestamp = 1900000000;

void requireCondition(bool condition, const std::string &message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

PublicKey publicKey(const std::string &suffix) {
  return KeyPair::createDeterministicBls12381KeyPair(
             "genesis-config-validator-key-" + suffix)
      .publicKey();
}

BootstrapValidatorConfig validator(const std::string &suffix) {
  return BootstrapValidatorConfig(publicKey(suffix), 1, 1,
                                  "genesis-validator-metadata-" + suffix);
}

GenesisConfig validGenesisConfig() {
  return GenesisConfig(NetworkParameters::developmentLocal(), kTimestamp,
                       {validator("a"), validator("b")}, "nodo-devnet-genesis");
}

void testNetworkParametersAreValidAndDeterministic() {
  const NetworkParameters params = NetworkParameters::developmentLocal();

  requireCondition(params.isValid(),
                   "Development network parameters should be valid.");

  requireCondition(!params.deterministicId().empty(),
                   "Valid network parameters should have deterministic id.");

  const NetworkParameters invalid("bad chain id with spaces", "nodo",
                                  "nodo/0.1", 60, 1, 2, 3, 100, 10);

  requireCondition(!invalid.isValid(), "Unsafe chain id should be invalid.");
}

void testGenesisConfigBuildsBlockchainAndValidatorRegistry() {
  const GenesisConfig config = validGenesisConfig();

  requireCondition(config.isValid(), "Valid genesis config should pass audit.");

  requireCondition(!config.deterministicId().empty(),
                   "Valid genesis config should have deterministic id.");

  const auto result = GenesisBuilder::build(config);

  requireCondition(result.built(),
                   "Genesis builder should build valid chain and registry.");

  requireCondition(result.blockchain().size() == 1U,
                   "Genesis blockchain should have exactly one block.");

  requireCondition(result.validatorRegistry().activeCount() == 2U,
                   "Validator registry should include bootstrap validators.");
}

void testGenesisRejectsTooFewValidators() {
  const NetworkParameters params("nodo-devnet-2", "nodo-devnet", "nodo/0.1", 60,
                                 3, 2, 3, 100, 10);

  const GenesisConfig config(params, kTimestamp, {validator("only-one")},
                             "nodo-devnet-genesis");

  requireCondition(!config.isValid(),
                   "Genesis config with too few validators should be invalid.");

  requireCondition(GenesisBuilder::build(config).status() ==
                       GenesisBuildStatus::INVALID_CONFIG,
                   "Genesis builder should reject invalid config.");
}

void testGenesisRejectsDuplicateBootstrapValidator() {
  const BootstrapValidatorConfig bootstrap = validator("duplicate");

  const GenesisConfig config(NetworkParameters::developmentLocal(), kTimestamp,
                             {bootstrap, bootstrap}, "nodo-devnet-genesis");

  requireCondition(!config.isValid(),
                   "Duplicate bootstrap validators should be invalid.");
}

void testGenesisAccountAllocationChangesDeterministicId() {
  const GenesisConfig withoutAccount = validGenesisConfig();

  const GenesisConfig withAccount(
      NetworkParameters::developmentLocal(), kTimestamp,
      {validator("a"), validator("b")},
      {GenesisAccountConfig("genesis-account-a", Amount::fromRawUnits(1000),
                            0)},
      "nodo-devnet-genesis");

  requireCondition(withAccount.isValid() && withoutAccount.deterministicId() !=
                                                withAccount.deterministicId(),
                   "Genesis account allocation should participate in "
                   "deterministic genesis id.");
}

void testGenesisRejectsDuplicateAccountAllocation() {
  const GenesisConfig config(
      NetworkParameters::developmentLocal(), kTimestamp,
      {validator("a"), validator("b")},
      {GenesisAccountConfig("same-account", Amount::fromRawUnits(1000), 0),
       GenesisAccountConfig("same-account", Amount::fromRawUnits(2000), 0)},
      "nodo-devnet-genesis");

  requireCondition(
      !config.isValid(),
      "Genesis config should reject duplicate account allocations.");
}

} // namespace

int main() {
  try {
    testNetworkParametersAreValidAndDeterministic();
    testGenesisConfigBuildsBlockchainAndValidatorRegistry();
    testGenesisRejectsTooFewValidators();
    testGenesisRejectsDuplicateBootstrapValidator();
    testGenesisAccountAllocationChangesDeterministicId();
    testGenesisRejectsDuplicateAccountAllocation();

    std::cout << "Nodo genesis config tests passed.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Nodo genesis config tests failed: " << error.what() << "\n";
    return 1;
  }
}
