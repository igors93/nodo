#include "node/RuntimeStartupService.hpp"
#include "config/GenesisDocumentCodec.hpp"
#include "config/GenesisRegistry.hpp"
#include "config/NetworkParameters.hpp"
#include "storage/AtomicFile.hpp"

#include "../common/TestnetCandidateGenesisFixture.hpp"

#include <cassert>
#include <filesystem>
#include <string>
#include <system_error>

namespace {

using nodo::config::GenesisLookupResult;
using nodo::config::NetworkParameters;
using nodo::node::RuntimeStartupService;
using nodo::node::StartupValidationResult;

std::filesystem::path tempPath(const std::string& suffix) {
    return std::filesystem::temp_directory_path() /
           ("nodo-runtime-startup-service-tests-" + suffix);
}

void clean(const std::filesystem::path& path) {
    std::error_code error;
    std::filesystem::remove_all(path, error);
}

void testResolveLocalnetSucceeds() {
    const GenesisLookupResult result =
        RuntimeStartupService::resolveGenesis("localnet");
    assert(result.found());
    assert(!result.genesis().deterministicId().empty());
}

// Without an operator genesis document, testnet-candidate cannot resolve.
void testResolveTestnetCandidateByNameFails() {
    const GenesisLookupResult result =
        RuntimeStartupService::resolveGenesis("testnet-candidate");
    assert(!result.found());
    assert(result.reason().find("--genesis-file") != std::string::npos);
}

void testResolveTestnetCandidateFromGenesisFile() {
    const std::filesystem::path root = tempPath("from-file");
    clean(root);
    const std::filesystem::path file =
        nodo::tests::writeTestnetCandidateTestGenesis(root / "genesis.nodo");

    const GenesisLookupResult result = RuntimeStartupService::resolveAndVerify(
        "testnet-candidate", root / "unused-data-dir", file);
    assert(result.found());
    assert(result.genesis().deterministicId() ==
           nodo::tests::testnetCandidateTestGenesis().deterministicId());

    clean(root);
}

// After init, commands read the genesis pinned in the data directory.
void testResolveTestnetCandidateFromDataDirectory() {
    const std::filesystem::path dataDir = tempPath("from-data-dir");
    clean(dataDir);
    nodo::tests::writeTestnetCandidateTestGenesis(
        nodo::node::NodeDataDirectoryConfig(dataDir).genesisConfigPath());

    const GenesisLookupResult result = RuntimeStartupService::resolveAndVerify(
        "testnet-candidate", dataDir, {});
    assert(result.found());
    assert(result.genesis().networkParameters().networkName() ==
           "testnet-candidate");

    clean(dataDir);
}

void testResolveTestnetCandidateWithoutDocumentFails() {
    const std::filesystem::path dataDir = tempPath("no-document");
    clean(dataDir);

    const GenesisLookupResult missingDirectory =
        RuntimeStartupService::resolveAndVerify("testnet-candidate", dataDir, {});
    assert(!missingDirectory.found());
    assert(missingDirectory.reason().find("nodo init --network testnet-candidate "
                                          "--genesis-file") != std::string::npos);

    const GenesisLookupResult missingFile =
        RuntimeStartupService::resolveAndVerify(
            "testnet-candidate", dataDir, dataDir / "absent.nodo");
    assert(!missingFile.found());
    assert(missingFile.reason().find("does not exist") != std::string::npos);
}

void testGenesisFileRejectedForBuiltInNetwork() {
    const std::filesystem::path root = tempPath("built-in");
    clean(root);
    const std::filesystem::path file =
        nodo::tests::writeTestnetCandidateTestGenesis(root / "genesis.nodo");

    const GenesisLookupResult result =
        RuntimeStartupService::resolveAndVerify("localnet", root, file);
    assert(!result.found());
    assert(result.reason().find("built-in genesis") != std::string::npos);

    clean(root);
}

// A document for another network must not satisfy testnet-candidate.
void testGenesisFileForOtherNetworkRejected() {
    const std::filesystem::path root = tempPath("other-network");
    clean(root);
    std::filesystem::create_directories(root);
    const std::filesystem::path file = root / "localnet-genesis.nodo";
    nodo::storage::AtomicFile::writeTextFile(
        file, nodo::config::GenesisDocumentCodec::encode(
                  nodo::config::GenesisRegistry::get("localnet").genesis()));

    const GenesisLookupResult result = RuntimeStartupService::resolveAndVerify(
        "testnet-candidate", root, file);
    assert(!result.found());
    assert(result.reason().find("declares network 'localnet'") !=
           std::string::npos);

    clean(root);
}

void testTamperedGenesisFileRejected() {
    const std::filesystem::path root = tempPath("tampered");
    clean(root);
    const std::filesystem::path file =
        nodo::tests::writeTestnetCandidateTestGenesis(root / "genesis.nodo");
    nodo::storage::AtomicFile::writeTextFile(
        file, nodo::storage::AtomicFile::readTextFile(file) +
                  "unexpectedField=1\n");

    const GenesisLookupResult result = RuntimeStartupService::resolveAndVerify(
        "testnet-candidate", root, file);
    assert(!result.found());
    assert(result.reason().find("is invalid") != std::string::npos);

    clean(root);
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
    const NetworkParameters params(nodo::config::NetworkParameterValues{
        .chainId = "nodo-mainnet-1", .networkName = "mainnet",
        .protocolVersion = "nodo/0.1", .epochDurationSeconds = 600,
        .minimumValidatorCount = 7, .quorumThresholdNumerator = 2,
        .quorumThresholdDenominator = 3, .maxTransactionsPerBlock = 250,
        .maxPeerCount = 256, .maxMempoolTransactions = 10000,
        .minimumFeeRawUnits = 10000, .targetBlockTimeSeconds = 15,
        .finalityDepth = 6});
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

void testResolveAndVerifyTestnetByNameFails() {
    const GenesisLookupResult result =
        RuntimeStartupService::resolveAndVerify("testnet-candidate");
    assert(!result.found());
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
    testResolveTestnetCandidateByNameFails();
    testResolveTestnetCandidateFromGenesisFile();
    testResolveTestnetCandidateFromDataDirectory();
    testResolveTestnetCandidateWithoutDocumentFails();
    testGenesisFileRejectedForBuiltInNetwork();
    testGenesisFileForOtherNetworkRejected();
    testTamperedGenesisFileRejected();
    testResolveMainnetFails();
    testResolveUnknownNetworkFails();
    testValidateLocalnetProfileValid();
    testValidateTestnetCandidateProfileValid();
    testValidateMainnetProfileBlocked();
    testResolveAndVerifyLocalnetSucceeds();
    testResolveAndVerifyTestnetByNameFails();
    testResolveAndVerifyMainnetFails();
    testResolveAndVerifyUnknownFails();
    return 0;
}
