#include "crypto/LocalIdentity.hpp"

#include "crypto/Address.hpp"
#include "crypto/AddressDerivation.hpp"

#include <utility>

namespace nodo::crypto {

NodeIdentity::NodeIdentity()
    : m_keyId(""),
      m_publicKey(),
      m_address("") {}

NodeIdentity::NodeIdentity(
    std::string keyId,
    PublicKey publicKey,
    std::string address
)
    : m_keyId(std::move(keyId)),
      m_publicKey(std::move(publicKey)),
      m_address(std::move(address)) {}

const std::string& NodeIdentity::keyId() const {
    return m_keyId;
}

const PublicKey& NodeIdentity::publicKey() const {
    return m_publicKey;
}

const std::string& NodeIdentity::address() const {
    return m_address;
}

bool NodeIdentity::isValid() const {
    return !m_keyId.empty() &&
           m_publicKey.isValid() &&
           Address::fromString(m_address).isValid() &&
           AddressDerivation::verifyAddressForPublicKey(
               Address::fromString(m_address),
               m_publicKey
           );
}

ValidatorIdentity::ValidatorIdentity()
    : m_keyId(""),
      m_publicKey(),
      m_validatorAddress("") {}

ValidatorIdentity::ValidatorIdentity(
    std::string keyId,
    PublicKey publicKey,
    std::string validatorAddress
)
    : m_keyId(std::move(keyId)),
      m_publicKey(std::move(publicKey)),
      m_validatorAddress(std::move(validatorAddress)) {}

const std::string& ValidatorIdentity::keyId() const {
    return m_keyId;
}

const PublicKey& ValidatorIdentity::publicKey() const {
    return m_publicKey;
}

const std::string& ValidatorIdentity::validatorAddress() const {
    return m_validatorAddress;
}

bool ValidatorIdentity::isValid() const {
    return !m_keyId.empty() &&
           m_publicKey.isValid() &&
           Address::fromString(m_validatorAddress).isValid() &&
           AddressDerivation::verifyAddressForPublicKey(
               Address::fromString(m_validatorAddress),
               m_publicKey
           );
}

} // namespace nodo::crypto
