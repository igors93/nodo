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
  const NetworkParameters params("nodo-mainnet-1", "mainnet", "nodo/0.1", 600,
                                 7, 2, 3, 250, 256, 10000, 10000, 15, 6,
                                 "NODO_CRYPTO_SUITE_V1", "NODO_STORAGE_V2");
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

} // namespace

int main() {
  testLocalnetIsDevelopment();
  testSoakLocalnetIsDevelopment();
  testTestnetCandidateIsStaging();
  testMainnetIsLockedProduction();
  testNetworkClassStringConversion();
  testDevelopmentLocalNotSafeForProduction();
  return 0;
}
