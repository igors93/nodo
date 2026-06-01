#include "crypto/NodeIdentity.hpp"

#include <sstream>

namespace nodo::crypto {

NodeIdentity::NodeIdentity()
    : m_nodeId(""),
      m_networkProfile(""),
      m_nodePublicKey(),
      m_createdAt(0) {}

NodeIdentity::NodeIdentity(
    std::string nodeId,
    std::string networkProfile,
    PublicKey nodePublicKey,
    std::int64_t createdAt
)
    : m_nodeId(std::move(nodeId)),
      m_networkProfile(std::move(networkProfile)),
      m_nodePublicKey(std::move(nodePublicKey)),
      m_createdAt(createdAt) {}

const std::string& NodeIdentity::nodeId() const { return m_nodeId; }
const std::string& NodeIdentity::networkProfile() const { return m_networkProfile; }
const PublicKey& NodeIdentity::nodePublicKey() const { return m_nodePublicKey; }
std::int64_t NodeIdentity::createdAt() const { return m_createdAt; }

bool NodeIdentity::isValid() const {
    return !m_nodeId.empty() &&
           !m_networkProfile.empty() &&
           m_nodePublicKey.isValid() &&
           m_createdAt > 0;
}

std::string NodeIdentity::serialize() const {
    std::ostringstream oss;
    oss << "NodeIdentity{"
        << "nodeId=" << m_nodeId
        << ";networkProfile=" << m_networkProfile
        << ";createdAt=" << m_createdAt
        << "}";
    return oss.str();
}

ValidatorIdentity::ValidatorIdentity()
    : m_validatorAddress(""),
      m_networkProfile(""),
      m_validatorPublicKey(),
      m_createdAt(0) {}

ValidatorIdentity::ValidatorIdentity(
    std::string validatorAddress,
    std::string networkProfile,
    PublicKey validatorPublicKey,
    std::int64_t createdAt
)
    : m_validatorAddress(std::move(validatorAddress)),
      m_networkProfile(std::move(networkProfile)),
      m_validatorPublicKey(std::move(validatorPublicKey)),
      m_createdAt(createdAt) {}

const std::string& ValidatorIdentity::validatorAddress() const { return m_validatorAddress; }
const std::string& ValidatorIdentity::networkProfile() const { return m_networkProfile; }
const PublicKey& ValidatorIdentity::validatorPublicKey() const { return m_validatorPublicKey; }
std::int64_t ValidatorIdentity::createdAt() const { return m_createdAt; }

bool ValidatorIdentity::isValid() const {
    return !m_validatorAddress.empty() &&
           !m_networkProfile.empty() &&
           m_validatorPublicKey.isValid() &&
           m_createdAt > 0;
}

std::string ValidatorIdentity::serialize() const {
    std::ostringstream oss;
    oss << "ValidatorIdentity{"
        << "address=" << m_validatorAddress
        << ";networkProfile=" << m_networkProfile
        << ";createdAt=" << m_createdAt
        << "}";
    return oss.str();
}

} // namespace nodo::crypto
