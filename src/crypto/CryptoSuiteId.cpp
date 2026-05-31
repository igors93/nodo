#include "crypto/CryptoSuiteId.hpp"

namespace nodo::crypto {

std::string cryptoSuiteIdToString(CryptoSuiteId suite) {
    switch (suite) {
        case CryptoSuiteId::NODO_CRYPTO_SUITE_V1:
            return "NODO_CRYPTO_SUITE_V1";
        case CryptoSuiteId::UNKNOWN:
        default:
            return "UNKNOWN";
    }
}

CryptoSuiteId cryptoSuiteIdFromString(const std::string& value) {
    if (value == "NODO_CRYPTO_SUITE_V1") {
        return CryptoSuiteId::NODO_CRYPTO_SUITE_V1;
    }

    return CryptoSuiteId::UNKNOWN;
}

bool isSupportedCryptoSuite(CryptoSuiteId suite) {
    return suite == CryptoSuiteId::NODO_CRYPTO_SUITE_V1;
}

} // namespace nodo::crypto
