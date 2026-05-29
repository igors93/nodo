#ifndef NODO_CRYPTO_POST_QUANTUM_ALGORITHM_PROFILE_HPP
#define NODO_CRYPTO_POST_QUANTUM_ALGORITHM_PROFILE_HPP

#include "crypto/CryptoAlgorithm.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::crypto {

/*
 * PostQuantumAlgorithmProfile describes a planned post-quantum signature
 * family known by Nodo.
 *
 * This is metadata, not an implementation.
 */
class PostQuantumAlgorithmProfile {
public:
    PostQuantumAlgorithmProfile();

    PostQuantumAlgorithmProfile(
        CryptoAlgorithm algorithm,
        std::string familyName,
        std::string standardTrackName,
        std::uint32_t claimedSecurityLevel,
        bool productionReady,
        std::string implementationStatus
    );

    CryptoAlgorithm algorithm() const;
    const std::string& familyName() const;
    const std::string& standardTrackName() const;
    std::uint32_t claimedSecurityLevel() const;
    bool productionReady() const;
    const std::string& implementationStatus() const;

    bool isValid() const;

    std::string serialize() const;

    static bool isPostQuantumProviderCandidate(
        CryptoAlgorithm algorithm
    );

    static PostQuantumAlgorithmProfile profileForAlgorithm(
        CryptoAlgorithm algorithm
    );

    static std::vector<PostQuantumAlgorithmProfile> knownProfiles();

private:
    CryptoAlgorithm m_algorithm;
    std::string m_familyName;
    std::string m_standardTrackName;
    std::uint32_t m_claimedSecurityLevel;
    bool m_productionReady;
    std::string m_implementationStatus;
};

} // namespace nodo::crypto

#endif
