#ifndef NODO_CRYPTO_AUDITED_SIGNATURE_PROVIDER_PROFILE_HPP
#define NODO_CRYPTO_AUDITED_SIGNATURE_PROVIDER_PROFILE_HPP

#include "crypto/CryptoAlgorithm.hpp"

#include <string>

namespace nodo::crypto {

/*
 * AuditedSignatureProviderProfile describes a signature provider that is meant
 * to be backed by a real audited implementation.
 *
 * This class does not prove that an implementation is audited. It creates the
 * metadata and validation gate that production providers must pass before Nodo
 * can treat them as usable.
 */
class AuditedSignatureProviderProfile {
public:
    AuditedSignatureProviderProfile();

    AuditedSignatureProviderProfile(
        CryptoAlgorithm algorithm,
        std::string providerName,
        std::string providerVersion,
        std::string auditReportReference,
        std::string implementationReference,
        bool productionReady
    );

    CryptoAlgorithm algorithm() const;
    const std::string& providerName() const;
    const std::string& providerVersion() const;
    const std::string& auditReportReference() const;
    const std::string& implementationReference() const;
    bool productionReady() const;

    bool isValid() const;
    bool allowsProductionUse() const;

    std::string serialize() const;

private:
    static bool isSafeReference(
        const std::string& value
    );

    CryptoAlgorithm m_algorithm;
    std::string m_providerName;
    std::string m_providerVersion;
    std::string m_auditReportReference;
    std::string m_implementationReference;
    bool m_productionReady;
};

} // namespace nodo::crypto

#endif
