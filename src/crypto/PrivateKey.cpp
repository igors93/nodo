#include "crypto/PrivateKey.hpp"

#include "crypto/Hex.hpp"

#include <utility>

namespace nodo::crypto {

PrivateKey::PrivateKey()
    : m_algorithm(CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE),
      m_keyMaterial("") {}

PrivateKey::PrivateKey(
    CryptoAlgorithm algorithm,
    std::string keyMaterial
)
    : m_algorithm(algorithm),
      m_keyMaterial(std::move(keyMaterial)) {}

CryptoAlgorithm PrivateKey::algorithm() const {
    return m_algorithm;
}

bool PrivateKey::isValid() const {
    if (m_keyMaterial.empty()) {
        return false;
    }

    if (m_algorithm == CryptoAlgorithm::CLASSIC_ED25519) {
        return hasHexByteSize(m_keyMaterial, 32);
    }

    if (m_algorithm == CryptoAlgorithm::BLS12_381) {
        return hasHexByteSize(m_keyMaterial, 32);
    }

    return !m_keyMaterial.empty();
}

const std::string& PrivateKey::keyMaterialForSigningOnly() const {
    return m_keyMaterial;
}

} // namespace nodo::crypto
