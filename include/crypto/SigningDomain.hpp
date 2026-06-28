#ifndef NODO_CRYPTO_SIGNING_DOMAIN_HPP
#define NODO_CRYPTO_SIGNING_DOMAIN_HPP

#include "crypto/CryptoPolicy.hpp"

#include <string>

namespace nodo::crypto {

enum class SigningDomain {
    UNKNOWN,
    PEER_HANDSHAKE,
    USER_TRANSACTION,
    VALIDATOR_VOTE,
    QUORUM_CERTIFICATE,
    MINT_AUTHORIZATION,
    TREASURY_PROPOSAL,
    GOVERNANCE_VOTE,
    VALIDATOR_BLOCK_PROPOSAL
};

std::string signingDomainToString(SigningDomain domain);
SigningDomain signingDomainFromString(const std::string& value);

SigningDomain signingDomainForSecurityContext(SecurityContext context);

bool isSigningDomainAllowedForContext(
    SigningDomain domain,
    SecurityContext context
);

} // namespace nodo::crypto

#endif
