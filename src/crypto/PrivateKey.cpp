#include "crypto/PrivateKey.hpp"

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
    /*
     * Validação mínima inicial.
     * No futuro, cada algoritmo terá tamanho e formato próprio de chave.
     */
    return !m_keyMaterial.empty();
}

const std::string& PrivateKey::keyMaterialForSigningOnly() const {
    return m_keyMaterial;
}

} // namespace nodo::crypto