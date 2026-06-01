#ifndef NODO_CRYPTO_NODE_IDENTITY_HPP
#define NODO_CRYPTO_NODE_IDENTITY_HPP

#include "crypto/PublicKey.hpp"

#include <cstdint>
#include <string>

namespace nodo::crypto {

/*
 * NodeIdentity represents the networking identity of a Nodo node.
 *
 * Security principle:
 * Node identity (used for P2P) is separated from validator identity
 * (used for consensus signing). A node may participate in the network
 * without being a validator. Mixing the two identities would allow a
 * compromised transport key to impersonate a validator.
 */
class NodeIdentity {
public:
    NodeIdentity();

    NodeIdentity(
        std::string nodeId,
        std::string networkProfile,
        PublicKey nodePublicKey,
        std::int64_t createdAt
    );

    const std::string& nodeId() const;
    const std::string& networkProfile() const;
    const PublicKey& nodePublicKey() const;
    std::int64_t createdAt() const;

    bool isValid() const;
    std::string serialize() const;

private:
    std::string m_nodeId;
    std::string m_networkProfile;
    PublicKey m_nodePublicKey;
    std::int64_t m_createdAt;
};

/*
 * ValidatorIdentity represents the consensus-signing identity of a validator.
 *
 * Security principle:
 * Validator identity must never be derived from the node transport key.
 * It uses a separate key pair so that a compromised node key cannot forge
 * validator votes or proposals.
 */
class ValidatorIdentity {
public:
    ValidatorIdentity();

    ValidatorIdentity(
        std::string validatorAddress,
        std::string networkProfile,
        PublicKey validatorPublicKey,
        std::int64_t createdAt
    );

    const std::string& validatorAddress() const;
    const std::string& networkProfile() const;
    const PublicKey& validatorPublicKey() const;
    std::int64_t createdAt() const;

    bool isValid() const;
    std::string serialize() const;

private:
    std::string m_validatorAddress;
    std::string m_networkProfile;
    PublicKey m_validatorPublicKey;
    std::int64_t m_createdAt;
};

} // namespace nodo::crypto

#endif
