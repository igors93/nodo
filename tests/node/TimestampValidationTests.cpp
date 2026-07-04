#include "node/RuntimeAccountStateBuilder.hpp"

#include "config/NetworkParameters.hpp"
#include "core/Blockchain.hpp"
#include "crypto/KeyPair.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using nodo::config::BootstrapValidatorConfig;
using nodo::config::GenesisBuilder;
using nodo::config::GenesisConfig;
using nodo::config::NetworkParameters;
using nodo::node::RuntimeAccountStateBuilder;

constexpr std::int64_t kNow = 1900000000LL;

void require(bool condition, const std::string &msg) {
  if (!condition)
    throw std::runtime_error(msg);
}

GenesisConfig minimalGenesisConfig() {
  return GenesisConfig(
      NetworkParameters::developmentLocal(), kNow,
      {BootstrapValidatorConfig(
          nodo::crypto::KeyPair::createDeterministicBls12381KeyPair("ts-val")
              .publicKey(),
          1, 1, "ts-val-meta")},
      "gap3-test");
}

const nodo::core::Blockchain &genesisChain() {
  static const GenesisConfig genesis = minimalGenesisConfig();
  static const auto result = GenesisBuilder::build(genesis);
  if (!result.built()) {
    throw std::runtime_error(
        "Genesis build failed in TimestampValidationTests");
  }
  return result.blockchain();
}

void testWallClockNowIsZeroByDefault() {
  const GenesisConfig genesis = minimalGenesisConfig();
  const nodo::core::Blockchain &chain = genesisChain();

  const auto context =
      RuntimeAccountStateBuilder::previewContextAtTip(genesis, chain, 0);

  require(context.wallClockNow() == 0,
          "Default wallClockNow must be 0 (timestamp check disabled).");
}

void testWallClockNowIsForwardedToContext() {
  const GenesisConfig genesis = minimalGenesisConfig();
  const nodo::core::Blockchain &chain = genesisChain();

  const std::int64_t wallClock = kNow + 3600;
  const auto context = RuntimeAccountStateBuilder::previewContextAtTip(
      genesis, chain, 0, wallClock);

  require(context.wallClockNow() == wallClock,
          "wallClockNow must be forwarded to StateTransitionPreviewContext.");
}

void testDifferentWallClockValuesProduceDifferentContexts() {
  const GenesisConfig genesis = minimalGenesisConfig();
  const nodo::core::Blockchain &chain = genesisChain();

  const auto ctx0 =
      RuntimeAccountStateBuilder::previewContextAtTip(genesis, chain, 0, 0);
  const auto ctxNow =
      RuntimeAccountStateBuilder::previewContextAtTip(genesis, chain, 0, kNow);

  require(ctx0.wallClockNow() != ctxNow.wallClockNow(),
          "Contexts built with different wallClockNow must differ.");
  require(ctx0.wallClockNow() == 0,
          "Zero wallClockNow context must have wallClockNow == 0.");
  require(ctxNow.wallClockNow() == kNow,
          "Non-zero wallClockNow context must carry the provided value.");
}

} // namespace

int main() {
  try {
    testWallClockNowIsZeroByDefault();
    testWallClockNowIsForwardedToContext();
    testDifferentWallClockValuesProduceDifferentContexts();

    std::cout << "Nodo Gap3 timestamp-validation tests passed.\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Nodo Gap3 timestamp-validation tests failed: " << e.what()
              << "\n";
    return 1;
  }
}
