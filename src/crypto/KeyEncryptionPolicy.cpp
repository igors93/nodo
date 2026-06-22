#include "crypto/KeyEncryptionPolicy.hpp"

namespace nodo::crypto {

std::string keyEncryptionLevelToString(KeyEncryptionLevel level) {
    switch (level) {
        case KeyEncryptionLevel::PLAINTEXT:      return "PLAINTEXT";
        case KeyEncryptionLevel::DEV_ENCRYPTED:  return "DEV_ENCRYPTED";
        case KeyEncryptionLevel::TESTNET_SAFE:   return "TESTNET_SAFE";
        default:                                 return "UNKNOWN";
    }
}

KeyEncryptionLevel KeyEncryptionPolicy::requiredLevelForNetwork(
    const std::string& networkName
) {
    if (networkName == "localnet") {
        return KeyEncryptionLevel::PLAINTEXT;
    }
    if (networkName == "testnet" || networkName == "testnet-candidate") {
        return KeyEncryptionLevel::TESTNET_SAFE;
    }
    // mainnet and any unknown profile: require TESTNET_SAFE as minimum.
    // mainnet is blocked entirely via isMainnetBlocked().
    return KeyEncryptionLevel::TESTNET_SAFE;
}

bool KeyEncryptionPolicy::isAcceptable(
    KeyEncryptionLevel actual,
    const std::string& networkName
) {
    if (isMainnetBlocked(networkName)) {
        return false;
    }
    const KeyEncryptionLevel required = requiredLevelForNetwork(networkName);
    return static_cast<int>(actual) >= static_cast<int>(required);
}

bool KeyEncryptionPolicy::isMainnetBlocked(
    const std::string& networkName
) {
    return networkName == "mainnet";
}

bool KeyEncryptionPolicy::isOfficialNetwork(
    const std::string& networkName
) {
    return networkName == "testnet" ||
           networkName == "testnet-candidate" ||
           networkName == "mainnet";
}

} // namespace nodo::crypto
