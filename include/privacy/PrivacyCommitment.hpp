#ifndef NODO_PRIVACY_PRIVACY_COMMITMENT_HPP
#define NODO_PRIVACY_PRIVACY_COMMITMENT_HPP

#include "utils/Amount.hpp"

#include <cstdint>
#include <string>

namespace nodo::privacy {

/*
 * PrivacyCommitmentType describes why a commitment exists.
 *
 * MINT_COMMITMENT:
 * A private coin commitment created from a public mint source.
 *
 * TRANSFER_OUTPUT_COMMITMENT:
 * A private output created by consuming previous private commitments.
 *
 * BURN_COMMITMENT:
 * A commitment proving that private value was intentionally destroyed.
 */
enum class PrivacyCommitmentType {
    MINT_COMMITMENT,
    TRANSFER_OUTPUT_COMMITMENT,
    BURN_COMMITMENT
};

std::string privacyCommitmentTypeToString(PrivacyCommitmentType type);

/*
 * PrivacyCommitment is an early foundation for private accounting.
 *
 * Important:
 * This is NOT real production privacy yet.
 *
 * Real privacy will require:
 * - cryptographic commitments;
 * - range proofs;
 * - nullifiers;
 * - zero-knowledge balance proofs;
 * - real hash and signature primitives.
 *
 * This class only creates the architectural boundary where those tools
 * will be connected later.
 */
class PrivacyCommitment {
public:
    /*
     * Creates a development commitment from visible data.
     *
     * Security warning:
     * The amount is accepted here only because this is an early local model.
     * Future production commitments must not reveal the amount publicly.
     */
    static PrivacyCommitment createDevelopmentCommitment(
        PrivacyCommitmentType type,
        std::string ownerAddress,
        utils::Amount amount,
        std::string blindingSecret,
        std::string sourceReference,
        std::int64_t timestamp
    );

    PrivacyCommitment(
        std::string id,
        PrivacyCommitmentType type,
        std::string commitmentHash,
        std::string ownerHint,
        std::string sourceReference,
        std::int64_t timestamp
    );

    const std::string& id() const;
    PrivacyCommitmentType type() const;
    const std::string& commitmentHash() const;
    const std::string& ownerHint() const;
    const std::string& sourceReference() const;
    std::int64_t timestamp() const;

    bool isValid() const;

    std::string serialize() const;

private:
    static std::string computeCommitmentHash(
        PrivacyCommitmentType type,
        const std::string& ownerAddress,
        const utils::Amount& amount,
        const std::string& blindingSecret,
        const std::string& sourceReference,
        std::int64_t timestamp
    );

    static std::string computeCommitmentId(
        PrivacyCommitmentType type,
        const std::string& commitmentHash,
        const std::string& sourceReference,
        std::int64_t timestamp
    );

    static std::string hashString(const std::string& value);

    std::string m_id;
    PrivacyCommitmentType m_type;
    std::string m_commitmentHash;
    std::string m_ownerHint;
    std::string m_sourceReference;
    std::int64_t m_timestamp;
};

} // namespace nodo::privacy

#endif