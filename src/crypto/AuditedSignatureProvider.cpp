#include "crypto/AuditedSignatureProvider.hpp"

namespace nodo::crypto {

bool AuditedSignatureProvider::isReadyForProductionUse() const {
    const AuditedSignatureProviderProfile profile =
        providerProfile();

    if (profile.algorithm() != algorithm()) {
        return false;
    }

    return profile.allowsProductionUse();
}

} // namespace nodo::crypto
