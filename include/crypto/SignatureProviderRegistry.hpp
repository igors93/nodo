#ifndef NODO_CRYPTO_SIGNATURE_PROVIDER_REGISTRY_HPP
#define NODO_CRYPTO_SIGNATURE_PROVIDER_REGISTRY_HPP

#include "crypto/AuditedSignatureProvider.hpp"
#include "crypto/CryptoAlgorithm.hpp"

#include <memory>
#include <vector>

namespace nodo::crypto {

/*
 * SignatureProviderRegistry stores audited provider adapters by algorithm.
 *
 * Development providers should not be registered here. This registry is for
 * real audited provider integrations only.
 */
class SignatureProviderRegistry {
public:
    void registerAuditedProvider(
        std::shared_ptr<const AuditedSignatureProvider> provider
    );

    bool hasAuditedProvider(
        CryptoAlgorithm algorithm
    ) const;

    bool hasProductionReadyProvider(
        CryptoAlgorithm algorithm
    ) const;

    const AuditedSignatureProvider& auditedProvider(
        CryptoAlgorithm algorithm
    ) const;

    std::size_t size() const;

private:
    std::vector<std::shared_ptr<const AuditedSignatureProvider>> m_providers;
};

} // namespace nodo::crypto

#endif
