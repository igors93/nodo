#include "config/NetworkProfileRegistry.hpp"

#include <stdexcept>

namespace nodo::config {

bool NetworkProfileRegistry::isKnown(const std::string& networkName) {
    return networkName == "localnet" ||
           networkName == "localnet-soak" ||
           networkName == "testnet-candidate" ||
           networkName == "mainnet";
}

bool NetworkProfileRegistry::isOfficialNetwork(const std::string& networkName) {
    return networkName == "testnet-candidate" ||
           networkName == "testnet" ||
           networkName == "mainnet";
}

bool NetworkProfileRegistry::isMainnetLocked(const std::string& networkName) {
    return networkName == "mainnet";
}

std::vector<std::string> NetworkProfileRegistry::knownProfiles() {
    return {
        "localnet",
        "localnet-soak",
        "testnet-candidate",
        "mainnet"
    };
}

NetworkParameters NetworkProfileRegistry::get(const std::string& networkName) {
    if (networkName == "localnet") {
        return NetworkParameters::developmentLocal();
    }
    if (networkName == "localnet-soak") {
        return NetworkParameters::developmentSoak();
    }
    if (networkName == "testnet-candidate") {
        return NetworkParameters::testnetCandidate();
    }
    if (networkName == "mainnet") {
        throw std::invalid_argument(
            "mainnet network profile is not available in this build. "
            "mainnet startup requires an audited genesis and production crypto providers."
        );
    }
    throw std::invalid_argument("Unknown network profile: " + networkName);
}

} // namespace nodo::config
