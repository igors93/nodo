#include "config/NetworkProfileRegistry.hpp"

#include <stdexcept>

namespace nodo::config {

bool NetworkProfileRegistry::isKnown(const std::string& networkName) {
    return networkName == "localnet" ||
           networkName == "nodo-localnet" ||
           networkName == "testnet-candidate" ||
           networkName == "nodo-testnet-candidate" ||
           networkName == "testnet" ||
           networkName == "nodo-testnet" ||
           networkName == "mainnet" ||
           networkName == "nodo-mainnet";
}

bool NetworkProfileRegistry::isOfficialNetwork(const std::string& networkName) {
    return networkName == "testnet-candidate" ||
           networkName == "nodo-testnet-candidate" ||
           networkName == "testnet" ||
           networkName == "nodo-testnet" ||
           networkName == "mainnet" ||
           networkName == "nodo-mainnet";
}

bool NetworkProfileRegistry::isMainnetLocked(const std::string& networkName) {
    return networkName == "mainnet" || networkName == "nodo-mainnet";
}

std::vector<std::string> NetworkProfileRegistry::knownProfiles() {
    return {
        "nodo-localnet",
        "nodo-testnet-candidate",
        "nodo-mainnet"
    };
}

NetworkParameters NetworkProfileRegistry::get(const std::string& networkName) {
    if (networkName == "localnet" || networkName == "nodo-localnet") {
        return NetworkParameters::developmentLocal();
    }
    if (networkName == "testnet-candidate" ||
        networkName == "nodo-testnet-candidate" ||
        networkName == "testnet" ||
        networkName == "nodo-testnet") {
        return NetworkParameters::testnetCandidate();
    }
    if (networkName == "mainnet" || networkName == "nodo-mainnet") {
        return NetworkParameters::mainnetPlaceholder();
    }
    throw std::invalid_argument("Unknown network profile: " + networkName);
}

} // namespace nodo::config
