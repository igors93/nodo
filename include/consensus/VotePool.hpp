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

class VotePool {
public:
    VotePool();

    VotePoolResult submitVote(const ValidatorVoteRecord& vote);

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

    bool hasConflictingVote(
        const std::string& validatorAddress,
        std::uint64_t blockIndex,
        std::uint64_t round
    ) const;

    // Returns all votes that were detected as double-votes (same
    // validator+height+round, different block hash). Used by DoubleVoteDetector
    // to submit slashing evidence to the EvidencePool.
    const std::vector<ValidatorVoteRecord>& conflictingVotes() const;

    // Returns a pointer to the first accepted vote from the given validator at
    // (blockIndex, round), or nullptr if no such vote exists. Used by
    // DoubleVoteDetector to form a DoubleVoteEvidence pair.
    const ValidatorVoteRecord* firstVoteForValidator(
        const std::string& validatorAddress,
        std::uint64_t blockIndex,
        std::uint64_t round
    ) const;

    std::size_t totalVoteCount() const;
    std::string serialize() const;

private:
    std::map<VotePoolBlockKey, std::vector<ValidatorVoteRecord>> m_votesByBlock;
    std::map<std::string, ValidatorVoteRecord> m_voteByValidatorHeightRound;
    std::set<std::string> m_voteIds;
    std::vector<ValidatorVoteRecord> m_conflictingVotes;

    static std::string validatorHeightRoundKey(const ValidatorVoteRecord& vote);
    static bool sameVoteDecision(const ValidatorVoteRecord& left, const ValidatorVoteRecord& right);
    static bool isMinimallyValidVote(const ValidatorVoteRecord& vote);
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
