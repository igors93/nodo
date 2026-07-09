#include "node/RuntimeStateVerifier.hpp"

#include "node/ProtocolStateTransition.hpp"

#include <cstdint>
#include <exception>
#include <limits>
#include <utility>

namespace nodo::node {

namespace {

std::int64_t minimumFeeRawUnits(
    const config::GenesisConfig& genesisConfig
) {
    const std::uint64_t minimumFee =
        genesisConfig.networkParameters().minimumFeeRawUnits();

    if (minimumFee > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return std::numeric_limits<std::int64_t>::max();
    }

    return static_cast<std::int64_t>(minimumFee);
}

} // namespace

RuntimeStateVerificationResult::RuntimeStateVerificationResult()
    : m_verified(false),
      m_reason("Uninitialized runtime state verification result."),
      m_latestStateRoot("") {}

RuntimeStateVerificationResult RuntimeStateVerificationResult::passed(
    std::string latestStateRoot
) {
    RuntimeStateVerificationResult result;
    result.m_verified = true;
    result.m_reason = "";
    result.m_latestStateRoot = std::move(latestStateRoot);
    return result;
}

RuntimeStateVerificationResult RuntimeStateVerificationResult::failed(
    std::string reason
) {
    RuntimeStateVerificationResult result;
    result.m_verified = false;
    result.m_reason = std::move(reason);
    result.m_latestStateRoot = "";
    return result;
}

bool RuntimeStateVerificationResult::verified() const {
    return m_verified;
}

const std::string& RuntimeStateVerificationResult::reason() const {
    return m_reason;
}

const std::string& RuntimeStateVerificationResult::latestStateRoot() const {
    return m_latestStateRoot;
}

RuntimeStateVerificationResult RuntimeStateVerifier::verifyManifestMatchesRuntime(
    const NodeRuntimeManifest& manifest,
    const NodeRuntime& runtime
) {
    if (manifest.chainId().empty()) {
        return RuntimeStateVerificationResult::failed("manifest chainId is empty");
    }

    if (manifest.networkName().empty()) {
        return RuntimeStateVerificationResult::failed("manifest networkName is empty");
    }

    if (manifest.genesisConfigId().empty()) {
        return RuntimeStateVerificationResult::failed("manifest genesisConfigId is empty");
    }

    if (!runtime.isValid()) {
        return RuntimeStateVerificationResult::failed("rebuilt runtime is invalid");
    }

    if (!runtime.blockchain().isValid()) {
        return RuntimeStateVerificationResult::failed("rebuilt blockchain is invalid");
    }

    if (runtime.config().genesisConfig().networkParameters().chainId() != manifest.chainId()) {
        return RuntimeStateVerificationResult::failed("manifest chainId does not match runtime network parameters");
    }

    if (runtime.config().genesisConfig().networkParameters().networkName() != manifest.networkName()) {
        return RuntimeStateVerificationResult::failed("manifest networkName does not match runtime network parameters");
    }

    if (runtime.config().genesisConfig().deterministicId() != manifest.genesisConfigId()) {
        return RuntimeStateVerificationResult::failed("manifest genesisConfigId does not match runtime genesis config");
    }

    if (runtime.blockchain().latestBlock().index() != manifest.latestBlockHeight()) {
        return RuntimeStateVerificationResult::failed("manifest latestBlockHeight does not match rebuilt blockchain");
    }

    if (runtime.blockchain().latestBlock().hash() != manifest.latestBlockHash()) {
        return RuntimeStateVerificationResult::failed("manifest latestBlockHash does not match rebuilt blockchain");
    }

    std::string latestStateRoot = runtime.blockchain().latestBlock().stateRoot();
    if (latestStateRoot.empty() && runtime.blockchain().size() == 1 && runtime.blockchain().latestBlock().isGenesisBlock()) {
        latestStateRoot = ProtocolStateTransition::initialReplayState(runtime.config().genesisConfig()).stateRoot;
    }

    if (latestStateRoot.empty()) {
        return RuntimeStateVerificationResult::failed("rebuilt latestStateRoot is empty");
    }

    if (manifest.latestStateRoot() != latestStateRoot) {
        return RuntimeStateVerificationResult::failed("manifest latestStateRoot does not match rebuilt protocol state");
    }

    return RuntimeStateVerificationResult::passed(latestStateRoot);
}

RuntimeStateVerificationResult RuntimeStateVerifier::verifyLatestStateRoot(
    const config::GenesisConfig& genesisConfig,
    const core::Blockchain& blockchain,
    const std::string& expectedLatestStateRoot
) {
    try {
        const std::string latestStateRoot =
            calculateLatestStateRoot(
                genesisConfig,
                blockchain
            );

        if (latestStateRoot.empty()) {
            return RuntimeStateVerificationResult::failed("rebuilt latestStateRoot is empty");
        }

        if (latestStateRoot != expectedLatestStateRoot) {
            return RuntimeStateVerificationResult::failed("Manifest latestStateRoot does not match rebuilt protocol state.");
        }

        return RuntimeStateVerificationResult::passed(latestStateRoot);
    } catch (const std::exception& error) {
        return RuntimeStateVerificationResult::failed(error.what());
    }
}

std::string RuntimeStateVerifier::calculateLatestStateRoot(
    const config::GenesisConfig& genesisConfig,
    const core::Blockchain& blockchain
) {
    return ProtocolStateTransition::replayToTip(
        genesisConfig,
        blockchain,
        minimumFeeRawUnits(genesisConfig)
    ).stateRoot;
}

} // namespace nodo::node
