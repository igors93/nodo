#include "crypto/PostQuantumMigrationPlan.hpp"

#include <sstream>
#include <utility>

namespace nodo::crypto {

PostQuantumMigrationPlan::PostQuantumMigrationPlan()
    : m_planVersion(""),
      m_classicAlgorithm(CryptoAlgorithm::CLASSIC_ED25519),
      m_postQuantumAlgorithm(CryptoAlgorithm::POST_QUANTUM_ML_DSA),
      m_hybridRequiredForCriticalOperations(false) {}

PostQuantumMigrationPlan::PostQuantumMigrationPlan(
    std::string planVersion,
    CryptoAlgorithm classicAlgorithm,
    CryptoAlgorithm postQuantumAlgorithm,
    bool hybridRequiredForCriticalOperations
)
    : m_planVersion(std::move(planVersion)),
      m_classicAlgorithm(classicAlgorithm),
      m_postQuantumAlgorithm(postQuantumAlgorithm),
      m_hybridRequiredForCriticalOperations(hybridRequiredForCriticalOperations) {}

PostQuantumMigrationPlan PostQuantumMigrationPlan::developmentHybridPlan() {
    return PostQuantumMigrationPlan(
        "NODO_PQ_MIGRATION_PLAN_V1",
        CryptoAlgorithm::CLASSIC_ED25519,
        CryptoAlgorithm::POST_QUANTUM_ML_DSA,
        false
    );
}

const std::string& PostQuantumMigrationPlan::planVersion() const {
    return m_planVersion;
}

CryptoAlgorithm PostQuantumMigrationPlan::classicAlgorithm() const {
    return m_classicAlgorithm;
}

CryptoAlgorithm PostQuantumMigrationPlan::postQuantumAlgorithm() const {
    return m_postQuantumAlgorithm;
}

bool PostQuantumMigrationPlan::hybridRequiredForCriticalOperations() const {
    return m_hybridRequiredForCriticalOperations;
}

bool PostQuantumMigrationPlan::isValid() const {
    if (m_planVersion.empty()) {
        return false;
    }

    if (!isClassicAlgorithm(m_classicAlgorithm)) {
        return false;
    }

    if (!PostQuantumAlgorithmProfile::isPostQuantumProviderCandidate(m_postQuantumAlgorithm)) {
        return false;
    }

    return true;
}

std::string PostQuantumMigrationPlan::serialize() const {
    std::ostringstream oss;

    oss << "PostQuantumMigrationPlan{"
        << "planVersion=" << m_planVersion
        << ";classicAlgorithm=" << cryptoAlgorithmToString(m_classicAlgorithm)
        << ";postQuantumAlgorithm=" << cryptoAlgorithmToString(m_postQuantumAlgorithm)
        << ";hybridRequiredForCriticalOperations="
        << (m_hybridRequiredForCriticalOperations ? "true" : "false")
        << "}";

    return oss.str();
}

} // namespace nodo::crypto
