#ifndef NODO_CONSENSUS_QUORUM_CERTIFICATE_HPP
#define NODO_CONSENSUS_QUORUM_CERTIFICATE_HPP

#include "consensus/ValidatorVoteRecord.hpp"
#include "core/ValidatorRegistry.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/SignatureProvider.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nodo::consensus {

enum class QuorumCertificateBuildStatus {
    CERTIFIED,
    INVALID_INPUT,
    INVALID_VALIDATOR_REGISTRY,
    INVALID_VOTE,
    UNREGISTERED_OR_INACTIVE_VOTER,
    DUPLICATE_VOTER,
    CONFLICTING_VOTE,
    NOT_ENOUGH_VALID_VOTES
};

std::string quorumCertificateBuildStatusToString(
    QuorumCertificateBuildStatus status
);

/*
 * QuorumCertificate is a deterministic proof that enough registered validators
 * approved the same block in the same consensus round.
 */
class QuorumCertificate {
public:
    QuorumCertificate();

    QuorumCertificate(
        std::uint64_t blockIndex,
        std::string blockHash,
        std::string previousHash,
        std::uint64_t round,
        std::uint64_t requiredVoteCount,
        std::uint64_t activeValidatorCount,
        std::vector<ValidatorVoteRecord> votes
    );

    std::uint64_t blockIndex() const;
    const std::string& blockHash() const;
    const std::string& previousHash() const;
    std::uint64_t round() const;
    std::uint64_t requiredVoteCount() const;
    std::uint64_t activeValidatorCount() const;
    const std::vector<ValidatorVoteRecord>& votes() const;

    std::size_t voteCount() const;

    bool isStructurallyValid() const;

    bool verify(
        const core::ValidatorRegistry& validatorRegistry,
        const crypto::CryptoPolicy& policy,
        const crypto::SignatureProvider& provider
    ) const;

    std::string serialize() const;

private:
    std::uint64_t m_blockIndex;
    std::string m_blockHash;
    std::string m_previousHash;
    std::uint64_t m_round;
    std::uint64_t m_requiredVoteCount;
    std::uint64_t m_activeValidatorCount;
    std::vector<ValidatorVoteRecord> m_votes;
};

class QuorumCertificateBuildResult {
public:
    QuorumCertificateBuildResult();

    static QuorumCertificateBuildResult certified(
        QuorumCertificate certificate
    );

    static QuorumCertificateBuildResult rejected(
        QuorumCertificateBuildStatus status,
        std::string reason
    );

    QuorumCertificateBuildStatus status() const;
    const std::string& reason() const;
    const QuorumCertificate& certificate() const;

    bool certified() const;

    std::string serialize() const;

private:
    QuorumCertificateBuildStatus m_status;
    std::string m_reason;
    QuorumCertificate m_certificate;
};

class QuorumCertificateBuilder {
public:
    static std::uint64_t requiredVoteCount(
        std::uint64_t activeValidatorCount,
        std::uint64_t thresholdNumerator,
        std::uint64_t thresholdDenominator
    );

    static QuorumCertificateBuildResult buildFromVotes(
        std::uint64_t blockIndex,
        const std::string& blockHash,
        const std::string& previousHash,
        std::uint64_t round,
        const std::vector<ValidatorVoteRecord>& candidateVotes,
        const core::ValidatorRegistry& validatorRegistry,
        const crypto::CryptoPolicy& policy,
        const crypto::SignatureProvider& provider,
        std::uint64_t thresholdNumerator = 2,
        std::uint64_t thresholdDenominator = 3
    );
};

} // namespace nodo::consensus

#endif
