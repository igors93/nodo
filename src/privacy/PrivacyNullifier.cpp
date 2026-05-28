#include "privacy/PrivacyNullifier.hpp"

#include "crypto/hash.h"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::privacy {

std::string privacyNullifierTypeToString(PrivacyNullifierType type) {
    switch (type) {
        case PrivacyNullifierType::SPEND_NULLIFIER:
            return "SPEND_NULLIFIER";

        case PrivacyNullifierType::BURN_NULLIFIER:
            return "BURN_NULLIFIER";

        default:
            return "UNKNOWN";
    }
}

PrivacyNullifier PrivacyNullifier::createDevelopmentNullifier(
    PrivacyNullifierType type,
    std::string commitmentId,
    std::string ownerSecret,
    std::string spendContext,
    std::int64_t createdAt
) {
    if (commitmentId.empty()) {
        throw std::invalid_argument("Privacy nullifier commitment id cannot be empty.");
    }

    if (ownerSecret.empty()) {
        throw std::invalid_argument("Privacy nullifier owner secret cannot be empty.");
    }

    if (spendContext.empty()) {
        throw std::invalid_argument("Privacy nullifier spend context cannot be empty.");
    }

    if (createdAt <= 0) {
        throw std::invalid_argument("Privacy nullifier timestamp must be positive.");
    }

    const std::string nullifierHash = computeNullifierHash(
        type,
        commitmentId,
        ownerSecret
    );

    const std::string contextHash = computeContextHash(
        spendContext,
        createdAt
    );

    const std::string id = computeNullifierId(
        type,
        nullifierHash
    );

    return PrivacyNullifier(
        id,
        type,
        nullifierHash,
        contextHash,
        createdAt
    );
}

PrivacyNullifier::PrivacyNullifier(
    std::string id,
    PrivacyNullifierType type,
    std::string nullifierHash,
    std::string contextHash,
    std::int64_t createdAt
)
    : m_id(std::move(id)),
      m_type(type),
      m_nullifierHash(std::move(nullifierHash)),
      m_contextHash(std::move(contextHash)),
      m_createdAt(createdAt) {}

const std::string& PrivacyNullifier::id() const {
    return m_id;
}

PrivacyNullifierType PrivacyNullifier::type() const {
    return m_type;
}

const std::string& PrivacyNullifier::nullifierHash() const {
    return m_nullifierHash;
}

const std::string& PrivacyNullifier::contextHash() const {
    return m_contextHash;
}

std::int64_t PrivacyNullifier::createdAt() const {
    return m_createdAt;
}

bool PrivacyNullifier::isValid() const {
    if (m_id.empty()) {
        return false;
    }

    if (m_nullifierHash.empty()) {
        return false;
    }

    if (m_contextHash.empty()) {
        return false;
    }

    if (m_createdAt <= 0) {
        return false;
    }

    if (m_id != computeNullifierId(m_type, m_nullifierHash)) {
        return false;
    }

    return true;
}

std::string PrivacyNullifier::serialize() const {
    std::ostringstream oss;

    oss << "PrivacyNullifier{"
        << "id=" << m_id
        << ";type=" << privacyNullifierTypeToString(m_type)
        << ";nullifierHash=" << m_nullifierHash
        << ";contextHash=" << m_contextHash
        << ";createdAt=" << m_createdAt
        << "}";

    return oss.str();
}

std::string PrivacyNullifier::computeNullifierHash(
    PrivacyNullifierType type,
    const std::string& commitmentId,
    const std::string& ownerSecret
) {
    std::ostringstream oss;

    /*
     * Deterministic development nullifier.
     *
     * Critical rule:
     * Do not include timestamp or spend context here.
     * The same private coin must always produce the same nullifier hash.
     */
    oss << "DevelopmentPrivacyNullifier{"
        << "type=" << privacyNullifierTypeToString(type)
        << ";commitmentId=" << commitmentId
        << ";ownerSecret=" << ownerSecret
        << "}";

    return hashString(oss.str());
}

std::string PrivacyNullifier::computeContextHash(
    const std::string& spendContext,
    std::int64_t createdAt
) {
    std::ostringstream oss;

    /*
     * Context hash can vary per spend attempt.
     * It is useful for audit/debug, but it must not define uniqueness.
     */
    oss << "PrivacyNullifierContext{"
        << "spendContext=" << spendContext
        << ";createdAt=" << createdAt
        << "}";

    return hashString(oss.str());
}

std::string PrivacyNullifier::computeNullifierId(
    PrivacyNullifierType type,
    const std::string& nullifierHash
) {
    std::ostringstream oss;

    oss << "PrivacyNullifierId{"
        << "type=" << privacyNullifierTypeToString(type)
        << ";nullifierHash=" << nullifierHash
        << "}";

    return hashString(oss.str());
}

std::string PrivacyNullifier::hashString(const std::string& value) {
    char output[65] = {0};
    nodo_hash_string(value.c_str(), output, sizeof(output));

    return std::string(output);
}

} // namespace nodo::privacy