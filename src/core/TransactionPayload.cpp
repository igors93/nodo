#include "core/TransactionPayload.hpp"

#include "crypto/CryptoAlgorithm.hpp"
#include "serialization/CanonicalReader.hpp"
#include "serialization/CanonicalWriter.hpp"

#include <stdexcept>
#include <utility>

namespace nodo::core {

namespace {
constexpr const char* REGISTRATION_SCHEMA = "NODO_VALIDATOR_REGISTRATION_PAYLOAD_V1";
constexpr const char* VOTE_SCHEMA = "NODO_GOVERNANCE_VOTE_PAYLOAD_V1";

bool safeScalar(const std::string& value, std::size_t limit = 256) {
    if (value.empty() || value.size() > limit) return false;
    for (char c : value) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.' || c == ':';
        if (!ok) return false;
    }
    return true;
}
}

ValidatorRegistrationPayload::ValidatorRegistrationPayload() = default;

ValidatorRegistrationPayload::ValidatorRegistrationPayload(
    crypto::PublicKey validatorPublicKey,
    std::string metadataHash
) : m_validatorPublicKey(std::move(validatorPublicKey)),
    m_metadataHash(std::move(metadataHash)) {}

const crypto::PublicKey& ValidatorRegistrationPayload::validatorPublicKey() const {
    return m_validatorPublicKey;
}
const std::string& ValidatorRegistrationPayload::metadataHash() const { return m_metadataHash; }
bool ValidatorRegistrationPayload::isValid() const {
    return m_validatorPublicKey.isValid() &&
           crypto::isValidatorAlgorithm(m_validatorPublicKey.algorithm()) &&
           safeScalar(m_metadataHash);
}

std::string ValidatorRegistrationPayload::serialize() const {
    if (!isValid()) throw std::invalid_argument("Invalid validator registration payload.");
    serialization::CanonicalWriter writer;
    writer.writeString(REGISTRATION_SCHEMA);
    writer.writeString(crypto::cryptoAlgorithmToString(m_validatorPublicKey.algorithm()));
    writer.writeString(m_validatorPublicKey.keyMaterial());
    writer.writeString(m_metadataHash);
    return writer.byteString();
}

ValidatorRegistrationPayload ValidatorRegistrationPayload::deserialize(
    const std::string& serialized
) {
    serialization::CanonicalReader reader(serialized, 4096);
    if (reader.readString() != REGISTRATION_SCHEMA) {
        throw std::invalid_argument("Unknown validator registration payload schema.");
    }
    ValidatorRegistrationPayload payload(
        crypto::PublicKey(
            crypto::cryptoAlgorithmFromString(reader.readString()),
            reader.readString()
        ),
        reader.readString()
    );
    reader.requireFullyConsumed();
    if (!payload.isValid() || payload.serialize() != serialized) {
        throw std::invalid_argument("Non-canonical validator registration payload.");
    }
    return payload;
}

std::string governanceVoteChoiceToString(GovernanceVoteChoice choice) {
    return choice == GovernanceVoteChoice::APPROVE ? "APPROVE" : "REJECT";
}

GovernanceVoteChoice governanceVoteChoiceFromString(const std::string& value) {
    if (value == "APPROVE") return GovernanceVoteChoice::APPROVE;
    if (value == "REJECT") return GovernanceVoteChoice::REJECT;
    throw std::invalid_argument("Unknown governance vote choice.");
}

GovernanceVotePayload::GovernanceVotePayload()
    : m_choice(GovernanceVoteChoice::REJECT) {}
GovernanceVotePayload::GovernanceVotePayload(
    std::string validatorAddress,
    GovernanceVoteChoice choice
) : m_validatorAddress(std::move(validatorAddress)), m_choice(choice) {}
const std::string& GovernanceVotePayload::validatorAddress() const { return m_validatorAddress; }
GovernanceVoteChoice GovernanceVotePayload::choice() const { return m_choice; }
bool GovernanceVotePayload::isValid() const { return safeScalar(m_validatorAddress); }

std::string GovernanceVotePayload::serialize() const {
    if (!isValid()) throw std::invalid_argument("Invalid governance vote payload.");
    serialization::CanonicalWriter writer;
    writer.writeString(VOTE_SCHEMA);
    writer.writeString(m_validatorAddress);
    writer.writeString(governanceVoteChoiceToString(m_choice));
    return writer.byteString();
}

GovernanceVotePayload GovernanceVotePayload::deserialize(const std::string& serialized) {
    serialization::CanonicalReader reader(serialized, 4096);
    if (reader.readString() != VOTE_SCHEMA) {
        throw std::invalid_argument("Unknown governance vote payload schema.");
    }
    GovernanceVotePayload payload(
        reader.readString(),
        governanceVoteChoiceFromString(reader.readString())
    );
    reader.requireFullyConsumed();
    if (!payload.isValid() || payload.serialize() != serialized) {
        throw std::invalid_argument("Non-canonical governance vote payload.");
    }
    return payload;
}

} // namespace nodo::core
