#include "crypto/ProtocolCryptoContext.hpp"

#include <utility>

namespace nodo::crypto {

ProtocolCryptoContext ProtocolCryptoContext::localnet() {
    return ProtocolCryptoContext(
        "localnet",
        CryptoPolicy::developmentPolicy(),
        false
    );
}

ProtocolCryptoContext::ProtocolCryptoContext(
    std::string networkProfile,
    CryptoPolicy policy,
    bool productionSafe
)
    : m_networkProfile(std::move(networkProfile)),
      m_policy(policy),
      m_productionSafe(productionSafe),
      m_localProvider() {}

const std::string& ProtocolCryptoContext::networkProfile() const {
    return m_networkProfile;
}

const CryptoPolicy& ProtocolCryptoContext::policy() const {
    return m_policy;
}

const SignatureProvider& ProtocolCryptoContext::signatureProvider() const {
    return m_localProvider;
}

bool ProtocolCryptoContext::productionSafe() const {
    return m_productionSafe;
}

bool ProtocolCryptoContext::isValid() const {
    return !m_networkProfile.empty() &&
           m_localProvider.algorithm() == CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE &&
           !m_productionSafe;
}

} // namespace nodo::crypto
