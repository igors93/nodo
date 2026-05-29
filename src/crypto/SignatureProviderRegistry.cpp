#include "crypto/SignatureProviderRegistry.hpp"

#include <stdexcept>

namespace nodo::crypto {

void SignatureProviderRegistry::registerAuditedProvider(
    std::shared_ptr<const AuditedSignatureProvider> provider
) {
    if (!provider) {
        throw std::invalid_argument("Cannot register null audited signature provider.");
    }

    const AuditedSignatureProviderProfile profile =
        provider->providerProfile();

    if (!profile.isValid()) {
        throw std::invalid_argument("Cannot register invalid audited signature provider profile.");
    }

    if (profile.algorithm() != provider->algorithm()) {
        throw std::invalid_argument("Audited signature provider algorithm mismatch.");
    }

    for (const auto& existingProvider : m_providers) {
        if (existingProvider->algorithm() == provider->algorithm()) {
            throw std::logic_error("Audited signature provider already registered for algorithm.");
        }
    }

    m_providers.push_back(std::move(provider));
}

bool SignatureProviderRegistry::hasAuditedProvider(
    CryptoAlgorithm algorithm
) const {
    for (const auto& provider : m_providers) {
        if (provider->algorithm() == algorithm) {
            return true;
        }
    }

    return false;
}

bool SignatureProviderRegistry::hasProductionReadyProvider(
    CryptoAlgorithm algorithm
) const {
    for (const auto& provider : m_providers) {
        if (provider->algorithm() == algorithm) {
            return provider->isReadyForProductionUse();
        }
    }

    return false;
}

const AuditedSignatureProvider& SignatureProviderRegistry::auditedProvider(
    CryptoAlgorithm algorithm
) const {
    for (const auto& provider : m_providers) {
        if (provider->algorithm() == algorithm) {
            return *provider;
        }
    }

    throw std::out_of_range("No audited signature provider registered for algorithm.");
}

std::size_t SignatureProviderRegistry::size() const {
    return m_providers.size();
}

} // namespace nodo::crypto
