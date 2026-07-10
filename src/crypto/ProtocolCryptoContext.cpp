#include "crypto/ProtocolCryptoContext.hpp"

#include "crypto/CryptoAlgorithm.hpp"

#include <utility>

namespace nodo::crypto {

namespace {

bool isLocalnetName(
    const std::string& networkName
) {
    return networkName == "localnet";
}

bool isTestnetName(
    const std::string& networkName
) {
    return networkName == "testnet" ||
           networkName == "testnet-candidate";
}

bool isMainnetName(
    const std::string& networkName
) {
    return networkName == "mainnet";
}

} // namespace

std::string protocolNetworkProfileToString(
    ProtocolNetworkProfile profile
) {
    switch (profile) {
        case ProtocolNetworkProfile::LOCALNET:
            return "localnet";
        case ProtocolNetworkProfile::TESTNET:
            return "testnet";
        case ProtocolNetworkProfile::MAINNET:
            return "mainnet";
        case ProtocolNetworkProfile::UNKNOWN:
            return "unknown";
        default:
            return "unknown";
    }
}

ProtocolCryptoContext ProtocolCryptoContext::localnet() {
    return ProtocolCryptoContext(
        ProtocolNetworkProfile::LOCALNET,
        "localnet",
        CryptoPolicy::developmentPolicy(),
        false,
        false,
        ""
    );
}

ProtocolCryptoContext ProtocolCryptoContext::testnet() {
    return ProtocolCryptoContext(
        ProtocolNetworkProfile::TESTNET,
        "testnet",
        CryptoPolicy::developmentPolicy(),
        false,
        false,
        ""
    );
}

ProtocolCryptoContext ProtocolCryptoContext::mainnet() {
    return ProtocolCryptoContext(
        ProtocolNetworkProfile::MAINNET,
        "mainnet",
        CryptoPolicy::developmentPolicy(),
        false,
        true,
        "mainnet requires a production-safe signature provider."
    );
}

ProtocolCryptoContext ProtocolCryptoContext::fromNetworkName(
    const std::string& networkName
) {
    if (isLocalnetName(networkName)) {
        return localnet();
    }

    if (networkName == "localnet-soak") {
        return ProtocolCryptoContext(
            ProtocolNetworkProfile::LOCALNET,
            networkName,
            CryptoPolicy::developmentPolicy(),
            false,
            false,
            ""
        );
    }

    if (isTestnetName(networkName)) {
        return testnet();
    }

    if (isMainnetName(networkName)) {
        return mainnet();
    }

    return ProtocolCryptoContext(
        ProtocolNetworkProfile::UNKNOWN,
        networkName,
        CryptoPolicy::developmentPolicy(),
        false,
        true,
        "Unknown network profile for protocol crypto context: " + networkName
    );
}

ProtocolCryptoContext::ProtocolCryptoContext(
    ProtocolNetworkProfile profile,
    std::string networkProfile,
    CryptoPolicy policy,
    bool temporaryProviderAllowed,
    bool requiresProductionProvider,
    std::string rejectionReason
)
    : m_profile(profile),
      m_networkProfile(std::move(networkProfile)),
      m_policy(policy),
      m_temporaryProviderAllowed(temporaryProviderAllowed),
      m_requiresProductionProvider(requiresProductionProvider),
      m_rejectionReason(std::move(rejectionReason)),
      m_userProvider(),
      m_validatorProvider() {}

ProtocolNetworkProfile ProtocolCryptoContext::profile() const {
    return m_profile;
}

const std::string& ProtocolCryptoContext::networkProfile() const {
    return m_networkProfile;
}

const CryptoPolicy& ProtocolCryptoContext::policy() const {
    return m_policy;
}

const SignatureProvider& ProtocolCryptoContext::signatureProvider() const {
    return m_validatorProvider;
}

const SignatureProvider& ProtocolCryptoContext::userSignatureProvider() const {
    return m_userProvider;
}

const SignatureProvider& ProtocolCryptoContext::validatorSignatureProvider() const {
    return m_validatorProvider;
}

bool ProtocolCryptoContext::temporaryProviderAllowed() const {
    return m_temporaryProviderAllowed;
}

bool ProtocolCryptoContext::requiresProductionProvider() const {
    return m_requiresProductionProvider;
}

bool ProtocolCryptoContext::productionSafe() const {
    return !hasTemporaryProvider() &&
           !m_policy.developmentMode() &&
           m_rejectionReason.empty();
}

bool ProtocolCryptoContext::hasTemporaryProvider() const {
    return isDevelopmentOnlyAlgorithm(m_userProvider.algorithm()) ||
           isDevelopmentOnlyAlgorithm(m_validatorProvider.algorithm());
}

bool ProtocolCryptoContext::isValid() const {
    if (m_networkProfile.empty() ||
        m_profile == ProtocolNetworkProfile::UNKNOWN) {
        return false;
    }

    if (hasTemporaryProvider() &&
        !m_temporaryProviderAllowed) {
        return false;
    }

    if (m_requiresProductionProvider &&
        !productionSafe()) {
        return false;
    }

    return m_rejectionReason.empty();
}

const std::string& ProtocolCryptoContext::rejectionReason() const {
    return m_rejectionReason;
}

} // namespace nodo::crypto
