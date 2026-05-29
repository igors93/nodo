#ifndef NODO_CRYPTO_POST_QUANTUM_MIGRATION_PLAN_HPP
#define NODO_CRYPTO_POST_QUANTUM_MIGRATION_PLAN_HPP

#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/PostQuantumAlgorithmProfile.hpp"

#include <string>
#include <vector>

namespace nodo::crypto {

/*
 * PostQuantumMigrationPlan describes how Nodo expects to move from
 * development/classic signatures toward hybrid signatures.
 *
 * This is a planning boundary. It does not activate post-quantum signing.
 */
class PostQuantumMigrationPlan {
public:
    PostQuantumMigrationPlan();

    PostQuantumMigrationPlan(
        std::string planVersion,
        CryptoAlgorithm classicAlgorithm,
        CryptoAlgorithm postQuantumAlgorithm,
        bool hybridRequiredForCriticalOperations
    );

    static PostQuantumMigrationPlan developmentHybridPlan();

    const std::string& planVersion() const;
    CryptoAlgorithm classicAlgorithm() const;
    CryptoAlgorithm postQuantumAlgorithm() const;
    bool hybridRequiredForCriticalOperations() const;

    bool isValid() const;

    std::string serialize() const;

private:
    std::string m_planVersion;
    CryptoAlgorithm m_classicAlgorithm;
    CryptoAlgorithm m_postQuantumAlgorithm;
    bool m_hybridRequiredForCriticalOperations;
};

} // namespace nodo::crypto

#endif
