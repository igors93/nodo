#include "crypto/AuditedSignatureProviderProfile.hpp"

#include <sstream>
#include <utility>

namespace nodo::crypto {

AuditedSignatureProviderProfile::AuditedSignatureProviderProfile()
    : m_algorithm(CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE),
      m_providerName(""),
      m_providerVersion(""),
      m_auditReportReference(""),
      m_implementationReference(""),
      m_productionReady(false) {}

AuditedSignatureProviderProfile::AuditedSignatureProviderProfile(
    CryptoAlgorithm algorithm,
    std::string providerName,
    std::string providerVersion,
    std::string auditReportReference,
    std::string implementationReference,
    bool productionReady
)
    : m_algorithm(algorithm),
      m_providerName(std::move(providerName)),
      m_providerVersion(std::move(providerVersion)),
      m_auditReportReference(std::move(auditReportReference)),
      m_implementationReference(std::move(implementationReference)),
      m_productionReady(productionReady) {}

CryptoAlgorithm AuditedSignatureProviderProfile::algorithm() const {
    return m_algorithm;
}

const std::string& AuditedSignatureProviderProfile::providerName() const {
    return m_providerName;
}

const std::string& AuditedSignatureProviderProfile::providerVersion() const {
    return m_providerVersion;
}

const std::string& AuditedSignatureProviderProfile::auditReportReference() const {
    return m_auditReportReference;
}

const std::string& AuditedSignatureProviderProfile::implementationReference() const {
    return m_implementationReference;
}

bool AuditedSignatureProviderProfile::productionReady() const {
    return m_productionReady;
}

bool AuditedSignatureProviderProfile::isValid() const {
    if (!isClassicAlgorithm(m_algorithm)) {
        return false;
    }

    if (m_algorithm == CryptoAlgorithm::DEVELOPMENT_FAKE_SIGNATURE) {
        return false;
    }

    if (!isSafeReference(m_providerName)) {
        return false;
    }

    if (!isSafeReference(m_providerVersion)) {
        return false;
    }

    if (!isSafeReference(m_auditReportReference)) {
        return false;
    }

    if (!isSafeReference(m_implementationReference)) {
        return false;
    }

    return true;
}

bool AuditedSignatureProviderProfile::allowsProductionUse() const {
    if (!isValid()) {
        return false;
    }

    if (!m_productionReady) {
        return false;
    }

    /*
     * "UNVERIFIED" is reserved for placeholder profiles and must never pass the
     * production gate.
     */
    if (m_auditReportReference == "UNVERIFIED") {
        return false;
    }

    if (m_implementationReference == "UNVERIFIED") {
        return false;
    }

    return true;
}

std::string AuditedSignatureProviderProfile::serialize() const {
    std::ostringstream oss;

    oss << "AuditedSignatureProviderProfile{"
        << "algorithm=" << cryptoAlgorithmToString(m_algorithm)
        << ";providerName=" << m_providerName
        << ";providerVersion=" << m_providerVersion
        << ";auditReportReference=" << m_auditReportReference
        << ";implementationReference=" << m_implementationReference
        << ";productionReady=" << (m_productionReady ? "true" : "false")
        << "}";

    return oss.str();
}

bool AuditedSignatureProviderProfile::isSafeReference(
    const std::string& value
) {
    if (value.empty()) {
        return false;
    }

    for (const char current : value) {
        const bool isAlpha =
            (current >= 'a' && current <= 'z') ||
            (current >= 'A' && current <= 'Z');

        const bool isDigit =
            current >= '0' && current <= '9';

        const bool isSafeSymbol =
            current == '_' ||
            current == '-' ||
            current == '.' ||
            current == ':' ||
            current == '/';

        if (!isAlpha && !isDigit && !isSafeSymbol) {
            return false;
        }
    }

    return true;
}

} // namespace nodo::crypto
