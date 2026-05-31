#include "crypto/CryptoAlgorithm.hpp"

namespace nodo::crypto {

std::string cryptoAlgorithmToString(CryptoAlgorithm algorithm) {
    switch (algorithm) {
        case CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE:
            return "DEVELOPMENT_FAKE_SIGNATURE";

        case CryptoAlgorithm::CLASSIC_ED25519:
            return "CLASSIC_ED25519";

        case CryptoAlgorithm::CLASSIC_ECDSA_SECP256K1:
            return "CLASSIC_ECDSA_SECP256K1";

        case CryptoAlgorithm::BLS12_381:
            return "BLS12_381";

        case CryptoAlgorithm::POST_QUANTUM_ML_DSA:
            return "POST_QUANTUM_ML_DSA";

        case CryptoAlgorithm::POST_QUANTUM_SLH_DSA:
            return "POST_QUANTUM_SLH_DSA";

        case CryptoAlgorithm::HYBRID_CLASSIC_AND_POST_QUANTUM:
            return "HYBRID_CLASSIC_AND_POST_QUANTUM";

        default:
            return "UNKNOWN";
    }
}

CryptoAlgorithm cryptoAlgorithmFromString(const std::string& value) {
    if (value == "DEVELOPMENT_FAKE_SIGNATURE") {
        return CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE;
    }

    if (value == "CLASSIC_ED25519") {
        return CryptoAlgorithm::CLASSIC_ED25519;
    }

    if (value == "CLASSIC_ECDSA_SECP256K1") {
        return CryptoAlgorithm::CLASSIC_ECDSA_SECP256K1;
    }

    if (value == "BLS12_381") {
        return CryptoAlgorithm::BLS12_381;
    }

    if (value == "POST_QUANTUM_ML_DSA") {
        return CryptoAlgorithm::POST_QUANTUM_ML_DSA;
    }

    if (value == "POST_QUANTUM_SLH_DSA") {
        return CryptoAlgorithm::POST_QUANTUM_SLH_DSA;
    }

    if (value == "HYBRID_CLASSIC_AND_POST_QUANTUM") {
        return CryptoAlgorithm::HYBRID_CLASSIC_AND_POST_QUANTUM;
    }

    return CryptoAlgorithm::HYBRID_CLASSIC_AND_POST_QUANTUM;
}

bool isClassicAlgorithm(CryptoAlgorithm algorithm) {
    return algorithm == CryptoAlgorithm::CLASSIC_ED25519 ||
           algorithm == CryptoAlgorithm::CLASSIC_ECDSA_SECP256K1;
}

bool isValidatorAlgorithm(CryptoAlgorithm algorithm) {
    return algorithm == CryptoAlgorithm::BLS12_381;
}

bool isPostQuantumAlgorithm(CryptoAlgorithm algorithm) {
    return algorithm == CryptoAlgorithm::POST_QUANTUM_ML_DSA ||
           algorithm == CryptoAlgorithm::POST_QUANTUM_SLH_DSA;
}

bool isDevelopmentOnlyAlgorithm(CryptoAlgorithm algorithm) {
    return algorithm == CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE;
}

} // namespace nodo::crypto
