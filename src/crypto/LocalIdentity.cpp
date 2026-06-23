#include "crypto/LocalIdentity.hpp"

#include "crypto/Address.hpp"
#include "crypto/AddressDerivation.hpp"

#include <utility>

namespace nodo::crypto {

LocalNodeKeyIdentity::LocalNodeKeyIdentity()
    : m_keyId(""),
      m_publicKey(),
      m_address("") {}

LocalNodeKeyIdentity::LocalNodeKeyIdentity(
    std::string keyId,
    PublicKey publicKey,
    std::string address
)
    : m_keyId(std::move(keyId)),
      m_publicKey(std::move(publicKey)),
      m_address(std::move(address)) {}

const std::string& LocalNodeKeyIdentity::keyId() const {
    return m_keyId;
}

const PublicKey& LocalNodeKeyIdentity::publicKey() const {
    return m_publicKey;
}

const std::string& LocalNodeKeyIdentity::address() const {
    return m_address;
}

bool LocalNodeKeyIdentity::isValid() const {
    return !m_keyId.empty() &&
           m_publicKey.isValid() &&
           Address::fromString(m_address).isValid() &&
           AddressDerivation::verifyAddressForPublicKey(
               Address::fromString(m_address),
               m_publicKey
           );
}

LocalValidatorKeyIdentity::LocalValidatorKeyIdentity()
    : m_keyId(""),
      m_publicKey(),
      m_validatorAddress("") {}

LocalValidatorKeyIdentity::LocalValidatorKeyIdentity(
    std::string keyId,
    PublicKey publicKey,
    std::string validatorAddress
)
    : m_keyId(std::move(keyId)),
      m_publicKey(std::move(publicKey)),
      m_validatorAddress(std::move(validatorAddress)) {}

const std::string& LocalValidatorKeyIdentity::keyId() const {
    return m_keyId;
}

const PublicKey& LocalValidatorKeyIdentity::publicKey() const {
    return m_publicKey;
}

const std::string& LocalValidatorKeyIdentity::validatorAddress() const {
    return m_validatorAddress;
}

bool LocalValidatorKeyIdentity::isValid() const {
    return !m_keyId.empty() &&
           m_publicKey.isValid() &&
           Address::fromString(m_validatorAddress).isValid() &&
           AddressDerivation::verifyAddressForPublicKey(
               Address::fromString(m_validatorAddress),
               m_publicKey
           );
}

} // namespace nodo::crypto
