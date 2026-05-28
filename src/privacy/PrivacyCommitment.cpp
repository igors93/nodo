#include "privacy/PrivacyCommitment.hpp"

#include "crypto/hash.h"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::privacy {

std::string privacyCommitmentTypeToString(PrivacyCommitmentType type) {
    switch (type) {
        case PrivacyCommitmentType::MINT_COMMITMENT:
            return "MINT_COMMITMENT";

        case PrivacyCommitmentType::TRANSFER_OUTPUT_COMMITMENT:
            return "TRANSFER_OUTPUT_COMMITMENT";

        case PrivacyCommitmentType::BURN_COMMITMENT:
            return "BURN_COMMITMENT";

        default:
            return "UNKNOWN";
    }
}

PrivacyCommitment PrivacyCommitment::createDevelopmentCommitment(
    PrivacyCommitmentType type,
    std::string ownerAddress,
    utils::Amount amount,
    std::string blindingSecret,
    std::string sourceReference,
    std::int64_t timestamp
) {
    if (ownerAddress.empty()) {
        throw std::invalid_argument("Privacy commitment owner address cannot be empty.");
    }

    if (!amount.isPositive()) {
        throw std::invalid_argument("Privacy commitment amount must be positive.");
    }

    if (blindingSecret.empty()) {
        throw std::invalid_argument("Privacy commitment blinding secret cannot be empty.");
    }

    if (sourceReference.empty()) {
        throw std::invalid_argument("Privacy commitment source reference cannot be empty.");
    }

    if (timestamp <= 0) {
        throw std::invalid_argument("Privacy commitment timestamp must be positive.");
    }

    const std::string commitmentHash = computeCommitmentHash(
        type,
        ownerAddress,
        amount,
        blindingSecret,
        sourceReference,
        timestamp
    );

    const std::string commitmentId = computeCommitmentId(
        type,
        commitmentHash,
        sourceReference,
        timestamp
    );

    /*
     * ownerHint is intentionally not the full owner address.
     *
     * This is not real privacy yet, but it starts separating public
     * commitment identity from direct account identity.
     */
    const std::string ownerHint = hashString("ownerHint:" + ownerAddress).substr(0, 16);

    return PrivacyCommitment(
        commitmentId,
        type,
        commitmentHash,
        ownerHint,
        sourceReference,
        timestamp
    );
}

PrivacyCommitment::PrivacyCommitment(
    std::string id,
    PrivacyCommitmentType type,
    std::string commitmentHash,
    std::string ownerHint,
    std::string sourceReference,
    std::int64_t timestamp
)
    : m_id(std::move(id)),
      m_type(type),
      m_commitmentHash(std::move(commitmentHash)),
      m_ownerHint(std::move(ownerHint)),
      m_sourceReference(std::move(sourceReference)),
      m_timestamp(timestamp) {}

const std::string& PrivacyCommitment::id() const {
    return m_id;
}

PrivacyCommitmentType PrivacyCommitment::type() const {
    return m_type;
}

const std::string& PrivacyCommitment::commitmentHash() const {
    return m_commitmentHash;
}

const std::string& PrivacyCommitment::ownerHint() const {
    return m_ownerHint;
}

const std::string& PrivacyCommitment::sourceReference() const {
    return m_sourceReference;
}

std::int64_t PrivacyCommitment::timestamp() const {
    return m_timestamp;
}

bool PrivacyCommitment::isValid() const {
    if (m_id.empty()) {
        return false;
    }

    if (m_commitmentHash.empty()) {
        return false;
    }

    if (m_ownerHint.empty()) {
        return false;
    }

    if (m_sourceReference.empty()) {
        return false;
    }

    if (m_timestamp <= 0) {
        return false;
    }

    if (m_id != computeCommitmentId(
            m_type,
            m_commitmentHash,
            m_sourceReference,
            m_timestamp
        )) {
        return false;
    }

    return true;
}

std::string PrivacyCommitment::serialize() const {
    std::ostringstream oss;

    oss << "PrivacyCommitment{"
        << "id=" << m_id
        << ";type=" << privacyCommitmentTypeToString(m_type)
        << ";commitmentHash=" << m_commitmentHash
        << ";ownerHint=" << m_ownerHint
        << ";sourceReference=" << m_sourceReference
        << ";timestamp=" << m_timestamp
        << "}";

    return oss.str();
}

std::string PrivacyCommitment::computeCommitmentHash(
    PrivacyCommitmentType type,
    const std::string& ownerAddress,
    const utils::Amount& amount,
    const std::string& blindingSecret,
    const std::string& sourceReference,
    std::int64_t timestamp
) {
    std::ostringstream oss;

    /*
     * Deterministic development commitment.
     *
     * Future production version:
     * Replace this with a real cryptographic commitment scheme.
     */
    oss << "DevelopmentPrivacyCommitment{"
        << "type=" << privacyCommitmentTypeToString(type)
        << ";ownerAddress=" << ownerAddress
        << ";amountRaw=" << amount.rawUnits()
        << ";blindingSecret=" << blindingSecret
        << ";sourceReference=" << sourceReference
        << ";timestamp=" << timestamp
        << "}";

    return hashString(oss.str());
}

std::string PrivacyCommitment::computeCommitmentId(
    PrivacyCommitmentType type,
    const std::string& commitmentHash,
    const std::string& sourceReference,
    std::int64_t timestamp
) {
    std::ostringstream oss;

    oss << "PrivacyCommitmentId{"
        << "type=" << privacyCommitmentTypeToString(type)
        << ";commitmentHash=" << commitmentHash
        << ";sourceReference=" << sourceReference
        << ";timestamp=" << timestamp
        << "}";

    return hashString(oss.str());
}

std::string PrivacyCommitment::hashString(const std::string& value) {
    char output[65] = {0};
    nodo_hash_string(value.c_str(), output, sizeof(output));

    return std::string(output);
}

} // namespace nodo::privacy