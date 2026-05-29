#include "crypto/PostQuantumAlgorithmProfile.hpp"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::crypto {

PostQuantumAlgorithmProfile::PostQuantumAlgorithmProfile()
    : m_algorithm(CryptoAlgorithm::POST_QUANTUM_ML_DSA),
      m_familyName(""),
      m_standardTrackName(""),
      m_claimedSecurityLevel(0),
      m_productionReady(false),
      m_implementationStatus("") {}

PostQuantumAlgorithmProfile::PostQuantumAlgorithmProfile(
    CryptoAlgorithm algorithm,
    std::string familyName,
    std::string standardTrackName,
    std::uint32_t claimedSecurityLevel,
    bool productionReady,
    std::string implementationStatus
)
    : m_algorithm(algorithm),
      m_familyName(std::move(familyName)),
      m_standardTrackName(std::move(standardTrackName)),
      m_claimedSecurityLevel(claimedSecurityLevel),
      m_productionReady(productionReady),
      m_implementationStatus(std::move(implementationStatus)) {}

CryptoAlgorithm PostQuantumAlgorithmProfile::algorithm() const {
    return m_algorithm;
}

const std::string& PostQuantumAlgorithmProfile::familyName() const {
    return m_familyName;
}

const std::string& PostQuantumAlgorithmProfile::standardTrackName() const {
    return m_standardTrackName;
}

std::uint32_t PostQuantumAlgorithmProfile::claimedSecurityLevel() const {
    return m_claimedSecurityLevel;
}

bool PostQuantumAlgorithmProfile::productionReady() const {
    return m_productionReady;
}

const std::string& PostQuantumAlgorithmProfile::implementationStatus() const {
    return m_implementationStatus;
}

bool PostQuantumAlgorithmProfile::isValid() const {
    if (!isPostQuantumProviderCandidate(m_algorithm)) {
        return false;
    }

    if (m_familyName.empty()) {
        return false;
    }

    if (m_standardTrackName.empty()) {
        return false;
    }

    if (m_claimedSecurityLevel == 0U) {
        return false;
    }

    if (m_implementationStatus.empty()) {
        return false;
    }

    /*
     * Nodo currently has only interface/profile support for post-quantum
     * providers. No post-quantum provider is production ready yet.
     */
    if (m_productionReady) {
        return false;
    }

    return true;
}

std::string PostQuantumAlgorithmProfile::serialize() const {
    std::ostringstream oss;

    oss << "PostQuantumAlgorithmProfile{"
        << "algorithm=" << cryptoAlgorithmToString(m_algorithm)
        << ";familyName=" << m_familyName
        << ";standardTrackName=" << m_standardTrackName
        << ";claimedSecurityLevel=" << m_claimedSecurityLevel
        << ";productionReady=" << (m_productionReady ? "true" : "false")
        << ";implementationStatus=" << m_implementationStatus
        << "}";

    return oss.str();
}

bool PostQuantumAlgorithmProfile::isPostQuantumProviderCandidate(
    CryptoAlgorithm algorithm
) {
    return algorithm == CryptoAlgorithm::POST_QUANTUM_ML_DSA ||
           algorithm == CryptoAlgorithm::POST_QUANTUM_SLH_DSA;
}

PostQuantumAlgorithmProfile PostQuantumAlgorithmProfile::profileForAlgorithm(
    CryptoAlgorithm algorithm
) {
    switch (algorithm) {
        case CryptoAlgorithm::POST_QUANTUM_ML_DSA:
            return PostQuantumAlgorithmProfile(
                CryptoAlgorithm::POST_QUANTUM_ML_DSA,
                "ML-DSA",
                "NIST post-quantum digital signature family",
                3,
                false,
                "INTERFACE_ONLY_NO_AUDITED_PROVIDER"
            );

        case CryptoAlgorithm::POST_QUANTUM_SLH_DSA:
            return PostQuantumAlgorithmProfile(
                CryptoAlgorithm::POST_QUANTUM_SLH_DSA,
                "SLH-DSA",
                "NIST post-quantum stateless hash-based signature family",
                3,
                false,
                "INTERFACE_ONLY_NO_AUDITED_PROVIDER"
            );

        default:
            throw std::invalid_argument("Unsupported post-quantum algorithm profile.");
    }
}

std::vector<PostQuantumAlgorithmProfile> PostQuantumAlgorithmProfile::knownProfiles() {
    return {
        profileForAlgorithm(CryptoAlgorithm::POST_QUANTUM_ML_DSA),
        profileForAlgorithm(CryptoAlgorithm::POST_QUANTUM_SLH_DSA)
    };
}

} // namespace nodo::crypto
