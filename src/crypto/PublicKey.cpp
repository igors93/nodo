#include "crypto/PublicKey.hpp"
#include "crypto/hash.h"

#include <sstream>
#include <utility>

namespace nodo::crypto {

PublicKey::PublicKey()
    : m_algorithm(CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE),
      m_keyMaterial("") {}

PublicKey::PublicKey(
    CryptoAlgorithm algorithm,
    std::string keyMaterial
)
    : m_algorithm(algorithm),
      m_keyMaterial(std::move(keyMaterial)) {}

CryptoAlgorithm PublicKey::algorithm() const {
    return m_algorithm;
}

const std::string& PublicKey::keyMaterial() const {
    return m_keyMaterial;
}

bool PublicKey::isValid() const {
    /*
     * Nesta primeira versão, a validação é simples:
     * a chave precisa ter conteúdo e algoritmo definido.
     *
     * No futuro, cada algoritmo terá validações próprias.
     */
    return !m_keyMaterial.empty();
}

std::string PublicKey::fingerprint() const {
    char output[65] = {0};
    const std::string data = serialize();

    nodo_hash_string(data.c_str(), output, sizeof(output));

    return std::string(output);
}

std::string PublicKey::serialize() const {
    std::ostringstream oss;

    oss << "PublicKey{"
        << "algorithm=" << cryptoAlgorithmToString(m_algorithm)
        << ";keyMaterial=" << m_keyMaterial
        << "}";

    return oss.str();
}

} // namespace nodo::crypto