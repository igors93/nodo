#ifndef NODO_CRYPTO_CRYPTO_SUITE_ID_HPP
#define NODO_CRYPTO_CRYPTO_SUITE_ID_HPP

#include <string>

namespace nodo::crypto {

enum class CryptoSuiteId {
    UNKNOWN,
    NODO_CRYPTO_SUITE_V1
};

std::string cryptoSuiteIdToString(CryptoSuiteId suite);
CryptoSuiteId cryptoSuiteIdFromString(const std::string& value);

bool isSupportedCryptoSuite(CryptoSuiteId suite);

} // namespace nodo::crypto

#endif
