#include "node/RuntimeStartupService.hpp"
#include "config/NetworkParameters.hpp"

#include <cassert>
#include <string>

namespace {

using nodo::config::GenesisLookupResult;
using nodo::config::NetworkParameters;
using nodo::node::RuntimeStartupService;
using nodo::node::StartupValidationResult;

void testResolveLocalnetSucceeds() {
    const GenesisLookupResult result =
        RuntimeStartupService::resolveGenesis("localnet");
    assert(result.found());
    assert(!result.genesis().deterministicId().empty());
}

void testResolveTestnetCandidateSucceeds() {
    const GenesisLookupResult result =
        RuntimeStartupService::resolveGenesis("testnet-candidate");
    assert(result.found());
}

void testResolveMainnetFails() {
    const GenesisLookupResult result =
        RuntimeStartupService::resolveGenesis("mainnet");
    assert(!result.found());
    assert(!result.reason().empty());
}

void testResolveUnknownNetworkFails() {
    const GenesisLookupResult result =
        RuntimeStartupService::resolveGenesis("does-not-exist");
    assert(!result.found());
    assert(!result.reason().empty());
}

void testValidateLocalnetProfileValid() {
    const NetworkParameters params = NetworkParameters::developmentLocal();
    const StartupValidationResult result =
        RuntimeStartupService::validateNetworkProfile(params);
    assert(result.valid());
}

void testValidateTestnetCandidateProfileValid() {
    const NetworkParameters params = NetworkParameters::testnetCandidate();
    const StartupValidationResult result =
        RuntimeStartupService::validateNetworkProfile(params);
    assert(result.valid());
}

void testValidateMainnetProfileBlocked() {
    const NetworkParameters params(
        "nodo-mainnet-1", "mainnet", "nodo/0.1",
        600, 7, 2, 3, 250, 256, 10000, 10000, 15, 6,
        "NODO_CRYPTO_SUITE_V1", "NODO_STORAGE_V2"
    );
    const StartupValidationResult result =
        RuntimeStartupService::validateNetworkProfile(params);
    assert(!result.valid());
    assert(!result.reason().empty());
}

void testResolveAndVerifyLocalnetSucceeds() {
    const GenesisLookupResult result =
        RuntimeStartupService::resolveAndVerify("localnet");
    assert(result.found());
    assert(!result.genesis().deterministicId().empty());
}

void testResolveAndVerifyTestnetSucceeds() {
    const GenesisLookupResult result =
        RuntimeStartupService::resolveAndVerify("testnet-candidate");
    assert(result.found());
}

void testResolveAndVerifyMainnetFails() {
    const GenesisLookupResult result =
        RuntimeStartupService::resolveAndVerify("mainnet");
    assert(!result.found());
    assert(!result.reason().empty());
    assert(result.reason().find("mainnet") != std::string::npos);
}

void testResolveAndVerifyUnknownFails() {
    const GenesisLookupResult result =
        RuntimeStartupService::resolveAndVerify("nonexistent-net");
    assert(!result.found());
}

} // namespace

int main() {
    testResolveLocalnetSucceeds();
    testResolveTestnetCandidateSucceeds();
    testResolveMainnetFails();
    testResolveUnknownNetworkFails();
    testValidateLocalnetProfileValid();
    testValidateTestnetCandidateProfileValid();
    testValidateMainnetProfileBlocked();
    testResolveAndVerifyLocalnetSucceeds();
    testResolveAndVerifyTestnetSucceeds();
    testResolveAndVerifyMainnetFails();
    testResolveAndVerifyUnknownFails();
    return 0;
}
