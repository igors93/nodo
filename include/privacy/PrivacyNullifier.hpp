#ifndef NODO_PRIVACY_PRIVACY_NULLIFIER_HPP
#define NODO_PRIVACY_PRIVACY_NULLIFIER_HPP

#include <cstdint>
#include <string>

namespace nodo::privacy {

/*
 * PrivacyNullifierType describes why a nullifier exists.
 *
 * SPEND_NULLIFIER:
 * Proves that a private commitment was consumed by a private spend.
 *
 * BURN_NULLIFIER:
 * Proves that private value was intentionally destroyed.
 */
enum class PrivacyNullifierType {
    SPEND_NULLIFIER,
    BURN_NULLIFIER
};

std::string privacyNullifierTypeToString(PrivacyNullifierType type);

/*
 * PrivacyNullifier is the public marker that prevents private double spending.
 *
 * Important:
 * This is NOT a production zero-knowledge nullifier yet.
 *
 * Security principle:
 * The nullifier hash must be deterministic for the same private coin and
 * owner secret. It must NOT depend on timestamp or transaction context.
 *
 * If timestamp or context were part of the nullifier hash, the same private
 * coin could produce multiple different nullifiers and double-spending would
 * not be detected.
 */
class PrivacyNullifier {
public:
    /*
     * Creates a development nullifier.
     *
     * Development-only model:
     * - commitmentId represents the private note being spent;
     * - ownerSecret represents the owner's private spend secret;
     * - spendContext is hashed separately and does not affect nullifierHash.
     */
    static PrivacyNullifier createDevelopmentNullifier(
        PrivacyNullifierType type,
        std::string commitmentId,
        std::string ownerSecret,
        std::string spendContext,
        std::int64_t createdAt
    );

    PrivacyNullifier(
        std::string id,
        PrivacyNullifierType type,
        std::string nullifierHash,
        std::string contextHash,
        std::int64_t createdAt
    );

    const std::string& id() const;
    PrivacyNullifierType type() const;
    const std::string& nullifierHash() const;
    const std::string& contextHash() const;
    std::int64_t createdAt() const;

    bool isValid() const;

    std::string serialize() const;

private:
    static std::string computeNullifierHash(
        PrivacyNullifierType type,
        const std::string& commitmentId,
        const std::string& ownerSecret
    );

    static std::string computeContextHash(
        const std::string& spendContext,
        std::int64_t createdAt
    );

    static std::string computeNullifierId(
        PrivacyNullifierType type,
        const std::string& nullifierHash
    );

    static std::string hashString(const std::string& value);

    std::string m_id;
    PrivacyNullifierType m_type;
    std::string m_nullifierHash;
    std::string m_contextHash;
    std::int64_t m_createdAt;
};

} // namespace nodo::privacy

#endif