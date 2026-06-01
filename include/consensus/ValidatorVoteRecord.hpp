#ifndef NODO_CONSENSUS_VALIDATOR_VOTE_RECORD_HPP
#define NODO_CONSENSUS_VALIDATOR_VOTE_RECORD_HPP

#include "crypto/CryptoPolicy.hpp"
#include "crypto/PrivateKey.hpp"
#include "crypto/PublicKey.hpp"
#include "crypto/SignatureBundle.hpp"
#include "crypto/SignatureProvider.hpp"

#include <cstdint>
#include <string>

namespace nodo::consensus {

/*
 * ValidatorVoteDecision is the explicit consensus decision carried by a vote.
 *
 * UNKNOWN is intentionally invalid for accepted votes. It exists only so default
 * constructed objects cannot accidentally become valid.
 */
enum class ValidatorVoteDecision {
    UNKNOWN,
    APPROVE,
    REJECT
};

std::string validatorVoteDecisionToString(
    ValidatorVoteDecision decision
);

/*
 * ValidatorVoteRecord is the first consensus vote primitive in Nodo.
 *
 * Security principle:
 * A vote must commit to the exact block height, block hash, previous hash,
 * consensus round, validator identity and timestamp. The signature is verified
 * over a deterministic payload built from those fields.
 */
class ValidatorVoteRecord {
public:
    ValidatorVoteRecord();

    ValidatorVoteRecord(
        std::string validatorAddress,
        crypto::PublicKey validatorPublicKey,
        std::uint64_t blockIndex,
        std::string blockHash,
        std::string previousHash,
        std::uint64_t round,
        ValidatorVoteDecision decision,
        std::string reasonHash,
        std::int64_t createdAt,
        crypto::SignatureBundle signatureBundle
    );

    const std::string& validatorAddress() const;
    const crypto::PublicKey& validatorPublicKey() const;
    std::uint64_t blockIndex() const;
    const std::string& blockHash() const;
    const std::string& previousHash() const;
    std::uint64_t round() const;
    ValidatorVoteDecision decision() const;
    const std::string& reasonHash() const;
    std::int64_t createdAt() const;
    const crypto::SignatureBundle& signatureBundle() const;

    std::string deterministicId() const;
    std::string signingPayload() const;

    bool matchesBlock(
        std::uint64_t blockIndex,
        const std::string& blockHash,
        std::uint64_t round
    ) const;

    bool isStructurallyValid(
        const crypto::CryptoPolicy& policy
    ) const;

    bool verify(
        const crypto::CryptoPolicy& policy,
        const crypto::SignatureProvider& provider
    ) const;

    std::string serialize() const;

    static std::string buildSigningPayload(
        const std::string& validatorAddress,
        const crypto::PublicKey& validatorPublicKey,
        std::uint64_t blockIndex,
        const std::string& blockHash,
        const std::string& previousHash,
        std::uint64_t round,
        ValidatorVoteDecision decision,
        const std::string& reasonHash,
        std::int64_t createdAt
    );

    static ValidatorVoteRecord createVote(
        const std::string& validatorAddress,
        const crypto::PublicKey& validatorPublicKey,
        const crypto::PrivateKey& validatorPrivateKey,
        std::uint64_t blockIndex,
        const std::string& blockHash,
        const std::string& previousHash,
        std::uint64_t round,
        ValidatorVoteDecision decision,
        const std::string& reasonHash,
        std::int64_t createdAt,
        const crypto::SignatureProvider& provider
    );

    static ValidatorVoteRecord deserialize(
        const std::string& serialized
    );

private:
    std::string m_validatorAddress;
    crypto::PublicKey m_validatorPublicKey;
    std::uint64_t m_blockIndex;
    std::string m_blockHash;
    std::string m_previousHash;
    std::uint64_t m_round;
    ValidatorVoteDecision m_decision;
    std::string m_reasonHash;
    std::int64_t m_createdAt;
    crypto::SignatureBundle m_signatureBundle;
};

} // namespace nodo::consensus

#endif
