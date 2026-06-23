#ifndef NODO_CRYPTO_LOCAL_IDENTITY_HPP
#define NODO_CRYPTO_LOCAL_IDENTITY_HPP

#include "crypto/PublicKey.hpp"

#include <string>

namespace nodo::crypto {

class LocalNodeKeyIdentity {
public:
    LocalNodeKeyIdentity();

    LocalNodeKeyIdentity(
        std::string keyId,
        PublicKey publicKey,
        std::string address
    );

    const std::string& keyId() const;
    const PublicKey& publicKey() const;
    const std::string& address() const;
    bool isValid() const;

private:
    std::string m_keyId;
    PublicKey m_publicKey;
    std::string m_address;
};

class LocalValidatorKeyIdentity {
public:
    LocalValidatorKeyIdentity();

    LocalValidatorKeyIdentity(
        std::string keyId,
        PublicKey publicKey,
        std::string validatorAddress
    );

    const std::string& keyId() const;
    const PublicKey& publicKey() const;
    const std::string& validatorAddress() const;
    bool isValid() const;

private:
    std::string m_keyId;
    PublicKey m_publicKey;
    std::string m_validatorAddress;
};

} // namespace nodo::crypto

#endif
