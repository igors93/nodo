#ifndef NODO_CORE_TRANSACTION_PAYLOAD_HPP
#define NODO_CORE_TRANSACTION_PAYLOAD_HPP

#include "crypto/PublicKey.hpp"

#include <string>

namespace nodo::core {

class ValidatorRegistrationPayload {
public:
    ValidatorRegistrationPayload();
    ValidatorRegistrationPayload(crypto::PublicKey validatorPublicKey, std::string metadataHash);

    const crypto::PublicKey& validatorPublicKey() const;
    const std::string& metadataHash() const;
    bool isValid() const;
    std::string serialize() const;
    static ValidatorRegistrationPayload deserialize(const std::string& serialized);

private:
    crypto::PublicKey m_validatorPublicKey;
    std::string m_metadataHash;
};

enum class GovernanceVoteChoice {
    APPROVE,
    REJECT
};

std::string governanceVoteChoiceToString(GovernanceVoteChoice choice);
GovernanceVoteChoice governanceVoteChoiceFromString(const std::string& value);

class GovernanceVotePayload {
public:
    GovernanceVotePayload();
    GovernanceVotePayload(std::string validatorAddress, GovernanceVoteChoice choice);

    const std::string& validatorAddress() const;
    GovernanceVoteChoice choice() const;
    bool isValid() const;
    std::string serialize() const;
    static GovernanceVotePayload deserialize(const std::string& serialized);

private:
    std::string m_validatorAddress;
    GovernanceVoteChoice m_choice;
};

} // namespace nodo::core

#endif
