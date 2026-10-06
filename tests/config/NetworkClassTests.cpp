#include "config/NetworkParameters.hpp"

#include <cassert>
#include <string>

namespace {

using nodo::config::NetworkClass;
using nodo::config::networkClassToString;
using nodo::config::NetworkParameters;

void testLocalnetIsDevelopment() {
  const NetworkParameters params = NetworkParameters::developmentLocal();
  assert(params.networkClass() == NetworkClass::DEVELOPMENT_LOCAL);
  assert(networkClassToString(params.networkClass()) == "DEVELOPMENT_LOCAL");
}

void testSoakLocalnetIsDevelopment() {
  const NetworkParameters params = NetworkParameters::developmentSoak();
  assert(params.networkClass() == NetworkClass::DEVELOPMENT_LOCAL);
  assert(params.networkName() == "localnet-soak");
}

void testTestnetCandidateIsStaging() {
  const NetworkParameters params = NetworkParameters::testnetCandidate();
  assert(params.networkClass() == NetworkClass::STAGING_CANDIDATE);
  assert(networkClassToString(params.networkClass()) == "STAGING_CANDIDATE");
}

void testMainnetIsLockedProduction() {
  const NetworkParameters params(nodo::config::NetworkParameterValues{
      .chainId = "nodo-mainnet-1", .networkName = "mainnet",
      .protocolVersion = "nodo/0.1", .epochDurationSeconds = 600,
      .minimumValidatorCount = 7, .quorumThresholdNumerator = 2,
      .quorumThresholdDenominator = 3, .maxTransactionsPerBlock = 250,
      .maxPeerCount = 256, .maxMempoolTransactions = 10000,
      .minimumFeeRawUnits = 10000, .targetBlockTimeSeconds = 15,
      .finalityDepth = 6});
  assert(params.networkClass() == NetworkClass::LOCKED_PRODUCTION);
  assert(networkClassToString(params.networkClass()) == "LOCKED_PRODUCTION");
}

void testNetworkClassStringConversion() {
  assert(networkClassToString(NetworkClass::DEVELOPMENT_LOCAL) ==
         "DEVELOPMENT_LOCAL");
  assert(networkClassToString(NetworkClass::STAGING_CANDIDATE) ==
         "STAGING_CANDIDATE");
  assert(networkClassToString(NetworkClass::LOCKED_PRODUCTION) ==
         "LOCKED_PRODUCTION");
}

void testDevelopmentLocalNotSafeForProduction() {
  const NetworkParameters devParams = NetworkParameters::developmentLocal();
  const NetworkParameters testnetParams = NetworkParameters::testnetCandidate();

  // Development local must not be the same class as staging candidate.
  assert(devParams.networkClass() != testnetParams.networkClass());

  // Staging candidate must not be the same class as development local.
  assert(testnetParams.networkClass() != NetworkClass::DEVELOPMENT_LOCAL);
}

void testUnsafeQuorumThresholdIsRejected() {
  const NetworkParameters params(nodo::config::NetworkParameterValues{
      .chainId = "nodo-unsafe-1", .networkName = "unsafe",
      .protocolVersion = "nodo/0.1", .epochDurationSeconds = 60,
      .minimumValidatorCount = 1, .quorumThresholdNumerator = 1,
      .quorumThresholdDenominator = 2, .maxTransactionsPerBlock = 1000,
      .maxPeerCount = 128});
  assert(!params.isValid());
}

} // namespace

int main() {
  testLocalnetIsDevelopment();
  testSoakLocalnetIsDevelopment();
  testTestnetCandidateIsStaging();
  testMainnetIsLockedProduction();
  testNetworkClassStringConversion();
  testDevelopmentLocalNotSafeForProduction();
  testUnsafeQuorumThresholdIsRejected();
  return 0;
}
