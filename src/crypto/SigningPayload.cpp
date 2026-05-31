#include "crypto/SigningPayload.hpp"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::crypto {

SigningPayload::SigningPayload(
    CryptoSuiteId suite,
    SigningDomain domain,
    std::string payload
)
    : m_suite(suite),
      m_domain(domain),
      m_payload(std::move(payload)) {}

CryptoSuiteId SigningPayload::suite() const {
    return m_suite;
}

SigningDomain SigningPayload::domain() const {
    return m_domain;
}

const std::string& SigningPayload::payload() const {
    return m_payload;
}

bool SigningPayload::isValid() const {
    return isSupportedCryptoSuite(m_suite) &&
           m_domain != SigningDomain::UNKNOWN &&
           !m_payload.empty();
}

std::string SigningPayload::canonicalMessage() const {
    if (!isValid()) {
        throw std::invalid_argument("Invalid signing payload.");
    }

    std::ostringstream oss;

    oss << "NODO_SIGNING_PAYLOAD_V1"
        << "|suite=" << cryptoSuiteIdToString(m_suite)
        << "|domain=" << signingDomainToString(m_domain)
        << "|payload_size=" << m_payload.size()
        << "|payload=" << m_payload;

    return oss.str();
}

} // namespace nodo::crypto
