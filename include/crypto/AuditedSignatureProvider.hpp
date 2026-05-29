#ifndef NODO_CRYPTO_AUDITED_SIGNATURE_PROVIDER_HPP
#define NODO_CRYPTO_AUDITED_SIGNATURE_PROVIDER_HPP

#include "crypto/AuditedSignatureProviderProfile.hpp"
#include "crypto/SignatureProvider.hpp"

namespace nodo::crypto {

/*
 * AuditedSignatureProvider is the boundary for future real audited signature
 * implementations.
 *
 * IMPORTANT:
 * This interface does not ship Ed25519/ECDSA code by itself. A real provider
 * must be backed by an audited implementation/library and must expose a valid
 * profile before Nodo can treat it as production-ready.
 */
class AuditedSignatureProvider : public SignatureProvider {
public:
    ~AuditedSignatureProvider() override = default;

    virtual AuditedSignatureProviderProfile providerProfile() const = 0;

    bool isReadyForProductionUse() const;
};

} // namespace nodo::crypto

#endif
