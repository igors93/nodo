#ifndef NODO_CRYPTO_SIGNING_PAYLOAD_HPP
#define NODO_CRYPTO_SIGNING_PAYLOAD_HPP

#include "crypto/CryptoSuiteId.hpp"
#include "crypto/SigningDomain.hpp"

#include <string>

namespace nodo::crypto {

class SigningPayload {
public:
    SigningPayload(
        CryptoSuiteId suite,
        SigningDomain domain,
        std::string payload
    );

    CryptoSuiteId suite() const;
    SigningDomain domain() const;
    const std::string& payload() const;

    bool isValid() const;
    std::string canonicalMessage() const;

private:
    CryptoSuiteId m_suite;
    SigningDomain m_domain;
    std::string m_payload;
};

} // namespace nodo::crypto

#endif
