#include "crypto/SigningDomain.hpp"

namespace nodo::crypto {

std::string signingDomainToString(SigningDomain domain) {
    switch (domain) {
        case SigningDomain::USER_TRANSACTION:
            return "NODO_TX_V1";
        case SigningDomain::VALIDATOR_VOTE:
            return "NODO_VALIDATOR_VOTE_V1";
        case SigningDomain::QUORUM_CERTIFICATE:
            return "NODO_QUORUM_CERTIFICATE_V1";
        case SigningDomain::MINT_AUTHORIZATION:
            return "NODO_MINT_AUTHORIZATION_V1";
        case SigningDomain::TREASURY_PROPOSAL:
            return "NODO_TREASURY_PROPOSAL_V1";
        case SigningDomain::GOVERNANCE_VOTE:
            return "NODO_GOVERNANCE_VOTE_V1";
        case SigningDomain::VALIDATOR_BLOCK_PROPOSAL:
            return "NODO_VALIDATOR_BLOCK_PROPOSAL_V1";
        case SigningDomain::UNKNOWN:
        default:
            return "UNKNOWN";
    }
}

SigningDomain signingDomainFromString(const std::string& value) {
    if (value == "NODO_TX_V1") {
        return SigningDomain::USER_TRANSACTION;
    }

    if (value == "NODO_VALIDATOR_VOTE_V1") {
        return SigningDomain::VALIDATOR_VOTE;
    }

    if (value == "NODO_QUORUM_CERTIFICATE_V1") {
        return SigningDomain::QUORUM_CERTIFICATE;
    }

    if (value == "NODO_MINT_AUTHORIZATION_V1") {
        return SigningDomain::MINT_AUTHORIZATION;
    }

    if (value == "NODO_TREASURY_PROPOSAL_V1") {
        return SigningDomain::TREASURY_PROPOSAL;
    }

    if (value == "NODO_GOVERNANCE_VOTE_V1") {
        return SigningDomain::GOVERNANCE_VOTE;
    }

    if (value == "NODO_VALIDATOR_BLOCK_PROPOSAL_V1") {
        return SigningDomain::VALIDATOR_BLOCK_PROPOSAL;
    }

    return SigningDomain::UNKNOWN;
}

SigningDomain signingDomainForSecurityContext(SecurityContext context) {
    switch (context) {
        case SecurityContext::USER_TRANSACTION:
            return SigningDomain::USER_TRANSACTION;
        case SecurityContext::VALIDATOR_OPERATION:
            return SigningDomain::VALIDATOR_VOTE;
        case SecurityContext::TREASURY_OPERATION:
            return SigningDomain::TREASURY_PROPOSAL;
        case SecurityContext::MINT_OPERATION:
            return SigningDomain::MINT_AUTHORIZATION;
        case SecurityContext::DEVELOPMENT_ONLY:
        default:
            return SigningDomain::UNKNOWN;
    }
}

bool isSigningDomainAllowedForContext(
    SigningDomain domain,
    SecurityContext context
) {
    if (domain == SigningDomain::UNKNOWN) {
        return false;
    }

    if (context == SecurityContext::VALIDATOR_OPERATION) {
        return domain == SigningDomain::VALIDATOR_VOTE ||
               domain == SigningDomain::VALIDATOR_BLOCK_PROPOSAL ||
               domain == SigningDomain::QUORUM_CERTIFICATE;
    }

    return domain == signingDomainForSecurityContext(context);
}

} // namespace nodo::crypto
