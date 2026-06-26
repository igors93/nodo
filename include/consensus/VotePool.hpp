#ifndef NODO_CONSENSUS_VOTE_POOL_HPP
#define NODO_CONSENSUS_VOTE_POOL_HPP

#include "consensus/QuorumCertificate.hpp"
#include "consensus/ValidatorVoteRecord.hpp"
#include "core/ValidatorRegistry.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/SignatureProvider.hpp"

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace nodo::consensus {

enum class VotePoolStatus {
    ACCEPTED,
    DUPLICATE,
    CONFLICTING_VOTE,
    INVALID_VOTE
};

std::string votePoolStatusToString(VotePoolStatus status);

class VotePoolResult {
public:
    VotePoolResult();
    VotePoolResult(VotePoolStatus status, std::string reason);

    VotePoolStatus status() const;
    const std::string& reason() const;
    bool accepted() const;
    bool duplicate() const;
    bool conflicting() const;
    bool success() const;

private:
    VotePoolStatus m_status;
    std::string m_reason;
};

class VotePoolBlockKey {
public:
    VotePoolBlockKey();
    VotePoolBlockKey(std::uint64_t blockIndex, std::string blockHash, std::uint64_t round);

    std::uint64_t blockIndex() const;
    const std::string& blockHash() const;
    std::uint64_t round() const;

    bool isValid() const;
    std::string serialize() const;
    bool operator<(const VotePoolBlockKey& other) const;

private:
    std::uint64_t m_blockIndex;
    std::string m_blockHash;
    std::uint64_t m_round;
};

class VotePoolQuorumProgress {
public:
    VotePoolQuorumProgress();

    VotePoolQuorumProgress(
        std::uint64_t blockIndex,
        std::string blockHash,
        std::uint64_t round,
        std::uint64_t acceptedVoteCount,
        std::uint64_t requiredVoteCount,
        std::uint64_t activeValidatorCount
    );

    std::uint64_t blockIndex() const;
    const std::string& blockHash() const;
    std::uint64_t round() const;
    std::uint64_t acceptedVoteCount() const;
    std::uint64_t requiredVoteCount() const;
    std::uint64_t activeValidatorCount() const;

    bool isValid() const;
    bool canCertify() const;
    std::string serialize() const;

private:
    std::uint64_t m_blockIndex;
    std::string m_blockHash;
    std::uint64_t m_round;
    std::uint64_t m_acceptedVoteCount;
    std::uint64_t m_requiredVoteCount;
    std::uint64_t m_activeValidatorCount;
};

class VotePool {
public:
    VotePool();

    VotePoolResult submitVote(
        const ValidatorVoteRecord& vote,
        const crypto::CryptoPolicy& policy,
        const crypto::SignatureProvider& provider
    );

    std::vector<ValidatorVoteRecord> votesForBlock(
        std::uint64_t blockIndex,
        const std::string& blockHash,
        std::uint64_t round
    ) const;

    std::size_t voteCountForBlock(
        std::uint64_t blockIndex,
        const std::string& blockHash,
        std::uint64_t round
    ) const;

    VotePoolQuorumProgress quorumProgressForBlock(
        std::uint64_t blockIndex,
        const std::string& blockHash,
        std::uint64_t round,
        std::uint64_t activeValidatorCount,
        std::uint64_t thresholdNumerator,
        std::uint64_t thresholdDenominator
    ) const;

    bool hasConflictingVote(
        const std::string& validatorAddress,
        std::uint64_t blockIndex,
        std::uint64_t round
    ) const;

    const std::vector<ValidatorVoteRecord>& conflictingVotes() const;

    const ValidatorVoteRecord* firstVoteForValidator(
        const std::string& validatorAddress,
        std::uint64_t blockIndex,
        std::uint64_t round,
        ValidatorVoteDecision decision
    ) const;

    std::size_t totalVoteCount() const;
    std::string serialize() const;
    void prune(std::uint64_t currentHeight);

private:
    std::map<VotePoolBlockKey, std::vector<ValidatorVoteRecord>> m_votesByBlock;
    std::map<std::string, ValidatorVoteRecord> m_voteByValidatorHeightRound;
    std::set<std::string> m_voteIds;
    std::vector<ValidatorVoteRecord> m_conflictingVotes;
    std::map<std::string, std::size_t> m_conflictingVoteCounts;

    static constexpr std::size_t kMaxConflictingVotesPerKey = 10;

    static std::string validatorHeightRoundKey(const ValidatorVoteRecord& vote);
    static bool sameVoteDecision(const ValidatorVoteRecord& left, const ValidatorVoteRecord& right);
    static bool isMinimallyValidVote(
        const ValidatorVoteRecord& vote,
        const crypto::CryptoPolicy& policy,
        const crypto::SignatureProvider& provider
    );
};

class QuorumAssembly {
public:
    static QuorumCertificateBuildResult tryBuildCertificate(
        const VotePool& votePool,
        std::uint64_t blockIndex,
        const std::string& blockHash,
        const std::string& previousHash,
        std::uint64_t round,
        const core::ValidatorRegistry& validatorRegistry,
        const crypto::CryptoPolicy& policy,
        const crypto::SignatureProvider& provider,
        std::uint64_t thresholdNumerator,
        std::uint64_t thresholdDenominator
    );
};

} // namespace nodo::consensus

#endif
