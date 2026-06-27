#include "consensus/VotePool.hpp"

#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::consensus {

std::string votePoolStatusToString(VotePoolStatus status) {
    switch (status) {
        case VotePoolStatus::ACCEPTED: return "ACCEPTED";
        case VotePoolStatus::DUPLICATE: return "DUPLICATE";
        case VotePoolStatus::CONFLICTING_VOTE: return "CONFLICTING_VOTE";
        case VotePoolStatus::INVALID_VOTE: return "INVALID_VOTE";
        default: return "INVALID_VOTE";
    }
}

VotePoolResult::VotePoolResult()
    : m_status(VotePoolStatus::INVALID_VOTE),
      m_reason("Uninitialized vote pool result.") {}

VotePoolResult::VotePoolResult(VotePoolStatus status, std::string reason)
    : m_status(status),
      m_reason(std::move(reason)) {}

VotePoolStatus VotePoolResult::status() const { return m_status; }
const std::string& VotePoolResult::reason() const { return m_reason; }
bool VotePoolResult::accepted() const { return m_status == VotePoolStatus::ACCEPTED; }
bool VotePoolResult::duplicate() const { return m_status == VotePoolStatus::DUPLICATE; }
bool VotePoolResult::conflicting() const { return m_status == VotePoolStatus::CONFLICTING_VOTE; }
bool VotePoolResult::success() const { return accepted() || duplicate(); }

VotePoolBlockKey::VotePoolBlockKey()
    : m_blockIndex(0),
      m_blockHash(""),
      m_round(0) {}

VotePoolBlockKey::VotePoolBlockKey(std::uint64_t blockIndex, std::string blockHash, std::uint64_t round)
    : m_blockIndex(blockIndex),
      m_blockHash(std::move(blockHash)),
      m_round(round) {}

std::uint64_t VotePoolBlockKey::blockIndex() const { return m_blockIndex; }
const std::string& VotePoolBlockKey::blockHash() const { return m_blockHash; }
std::uint64_t VotePoolBlockKey::round() const { return m_round; }

bool VotePoolBlockKey::isValid() const {
    return m_blockIndex > 0 && !m_blockHash.empty() && m_round > 0;
}

std::string VotePoolBlockKey::serialize() const {
    std::ostringstream output;
    output << "VotePoolBlockKey{"
           << "blockIndex=" << m_blockIndex
           << ";blockHash=" << m_blockHash
           << ";round=" << m_round
           << "}";
    return output.str();
}

bool VotePoolBlockKey::operator<(const VotePoolBlockKey& other) const {
    if (m_blockIndex != other.m_blockIndex) {
        return m_blockIndex < other.m_blockIndex;
    }

    if (m_round != other.m_round) {
        return m_round < other.m_round;
    }

    return m_blockHash < other.m_blockHash;
}

VotePoolQuorumProgress::VotePoolQuorumProgress()
    : m_blockIndex(0),
      m_blockHash(""),
      m_round(0),
      m_acceptedVoteCount(0),
      m_requiredVoteCount(0),
      m_activeValidatorCount(0) {}

VotePoolQuorumProgress::VotePoolQuorumProgress(
    std::uint64_t blockIndex,
    std::string blockHash,
    std::uint64_t round,
    std::uint64_t acceptedVoteCount,
    std::uint64_t requiredVoteCount,
    std::uint64_t activeValidatorCount
) : m_blockIndex(blockIndex),
    m_blockHash(std::move(blockHash)),
    m_round(round),
    m_acceptedVoteCount(acceptedVoteCount),
    m_requiredVoteCount(requiredVoteCount),
    m_activeValidatorCount(activeValidatorCount) {}

std::uint64_t VotePoolQuorumProgress::blockIndex() const { return m_blockIndex; }
const std::string& VotePoolQuorumProgress::blockHash() const { return m_blockHash; }
std::uint64_t VotePoolQuorumProgress::round() const { return m_round; }
std::uint64_t VotePoolQuorumProgress::acceptedVoteCount() const { return m_acceptedVoteCount; }
std::uint64_t VotePoolQuorumProgress::requiredVoteCount() const { return m_requiredVoteCount; }
std::uint64_t VotePoolQuorumProgress::activeValidatorCount() const { return m_activeValidatorCount; }

bool VotePoolQuorumProgress::isValid() const {
    return m_blockIndex > 0 &&
           !m_blockHash.empty() &&
           m_round > 0 &&
           m_requiredVoteCount > 0 &&
           m_activeValidatorCount > 0 &&
           m_requiredVoteCount <= m_activeValidatorCount &&
           m_acceptedVoteCount <= m_activeValidatorCount;
}

bool VotePoolQuorumProgress::canCertify() const {
    return isValid() && m_acceptedVoteCount >= m_requiredVoteCount;
}

std::string VotePoolQuorumProgress::serialize() const {
    std::ostringstream output;
    output << "VotePoolQuorumProgress{"
           << "blockIndex=" << m_blockIndex
           << ";blockHash=" << m_blockHash
           << ";round=" << m_round
           << ";acceptedVoteCount=" << m_acceptedVoteCount
           << ";requiredVoteCount=" << m_requiredVoteCount
           << ";activeValidatorCount=" << m_activeValidatorCount
           << ";canCertify=" << (canCertify() ? "1" : "0")
           << "}";
    return output.str();
}

VotePool::VotePool()
    : m_votesByBlock(),
      m_voteByValidatorHeightRound(),
      m_voteIds(),
      m_conflictingVotes() {}

VotePoolResult VotePool::submitVote(
    const ValidatorVoteRecord& vote,
    const crypto::CryptoPolicy& policy,
    const crypto::SignatureProvider& provider
) {
    if (!isMinimallyValidVote(vote, policy, provider)) {
        return VotePoolResult(VotePoolStatus::INVALID_VOTE, "Vote failed minimal pool validation.");
    }

    const std::string voteId =
        vote.deterministicId();

    if (voteId.empty()) {
        return VotePoolResult(VotePoolStatus::INVALID_VOTE, "Vote deterministic id is empty.");
    }

    if (m_voteIds.find(voteId) != m_voteIds.end()) {
        return VotePoolResult(VotePoolStatus::DUPLICATE, "Validator vote id already exists in pool.");
    }

    const std::string validatorKey = validatorHeightRoundKey(vote);
    const auto existing = m_voteByValidatorHeightRound.find(validatorKey);

    if (existing != m_voteByValidatorHeightRound.end()) {
        if (sameVoteDecision(existing->second, vote)) {
            return VotePoolResult(VotePoolStatus::DUPLICATE, "Validator vote already exists in pool.");
        }

        const auto countIt = m_conflictingVoteCounts.find(validatorKey);
        const std::size_t count = (countIt != m_conflictingVoteCounts.end()) ? countIt->second : 0;
        if (count >= kMaxConflictingVotesPerKey) {
            return VotePoolResult(VotePoolStatus::CONFLICTING_VOTE, "Validator submitted a conflicting vote.");
        }
        m_conflictingVotes.push_back(vote);
        ++m_conflictingVoteCounts[validatorKey];
        return VotePoolResult(VotePoolStatus::CONFLICTING_VOTE, "Validator submitted a conflicting vote.");
    }

    VotePoolBlockKey blockKey(vote.blockIndex(), vote.blockHash(), vote.round());
    m_votesByBlock[blockKey].push_back(vote);
    m_voteByValidatorHeightRound[validatorKey] = vote;
    m_voteIds.insert(voteId);

    return VotePoolResult(VotePoolStatus::ACCEPTED, "Vote accepted into pool.");
}

std::vector<ValidatorVoteRecord> VotePool::votesForBlock(
    std::uint64_t blockIndex,
    const std::string& blockHash,
    std::uint64_t round
) const {
    const VotePoolBlockKey key(blockIndex, blockHash, round);
    const auto found = m_votesByBlock.find(key);
    return found == m_votesByBlock.end() ? std::vector<ValidatorVoteRecord>() : found->second;
}

std::size_t VotePool::voteCountForBlock(
    std::uint64_t blockIndex,
    const std::string& blockHash,
    std::uint64_t round
) const {
    return votesForBlock(blockIndex, blockHash, round).size();
}

VotePoolQuorumProgress VotePool::quorumProgressForBlock(
    std::uint64_t blockIndex,
    const std::string& blockHash,
    std::uint64_t round,
    std::uint64_t activeValidatorCount,
    std::uint64_t thresholdNumerator,
    std::uint64_t thresholdDenominator
) const {
    const std::size_t observedVotes =
        voteCountForBlock(blockIndex, blockHash, round);

    if (observedVotes > std::numeric_limits<std::uint64_t>::max()) {
        throw std::overflow_error("Vote count cannot fit in uint64.");
    }

    const std::uint64_t requiredVotes =
        QuorumCertificateBuilder::requiredVoteCount(
            activeValidatorCount,
            thresholdNumerator,
            thresholdDenominator
        );

    return VotePoolQuorumProgress(
        blockIndex,
        blockHash,
        round,
        static_cast<std::uint64_t>(observedVotes),
        requiredVotes,
        activeValidatorCount
    );
}

bool VotePool::hasConflictingVote(
    const std::string& validatorAddress,
    std::uint64_t blockIndex,
    std::uint64_t round
) const {
    for (const auto& vote : m_conflictingVotes) {
        if (vote.validatorAddress() == validatorAddress &&
            vote.blockIndex() == blockIndex &&
            vote.round() == round) {
            return true;
        }
    }
    return false;
}

const std::vector<ValidatorVoteRecord>& VotePool::conflictingVotes() const {
    return m_conflictingVotes;
}

const ValidatorVoteRecord* VotePool::firstVoteForValidator(
    const std::string& validatorAddress,
    std::uint64_t blockIndex,
    std::uint64_t round,
    ValidatorVoteDecision decision
) const {
    const std::string key =
        validatorAddress
        + "#"
        + std::to_string(blockIndex)
        + "#"
        + std::to_string(round)
        + "#"
        + validatorVoteDecisionToString(decision);
    const auto it = m_voteByValidatorHeightRound.find(key);
    if (it == m_voteByValidatorHeightRound.end()) {
        return nullptr;
    }
    return &it->second;
}

std::size_t VotePool::totalVoteCount() const {
    return m_voteByValidatorHeightRound.size();
}

std::string VotePool::serialize() const {
    std::ostringstream output;
    output << "VotePool{"
           << "totalVoteCount=" << totalVoteCount()
           << ";conflictingVoteCount=" << m_conflictingVotes.size()
           << ";blocks=[";
    bool first = true;
    for (const auto& [key, votes] : m_votesByBlock) {
        if (!first) {
            output << ",";
        }
        output << key.serialize() << "#voteCount=" << votes.size();
        first = false;
    }
    output << "]}";
    return output.str();
}

std::string VotePool::validatorHeightRoundKey(const ValidatorVoteRecord& vote) {
    return vote.validatorAddress()
        + "#"
        + std::to_string(vote.blockIndex())
        + "#"
        + std::to_string(vote.round())
        + "#"
        + validatorVoteDecisionToString(vote.decision());
}

bool VotePool::sameVoteDecision(const ValidatorVoteRecord& left, const ValidatorVoteRecord& right) {
    return left.validatorAddress() == right.validatorAddress() &&
           left.blockIndex() == right.blockIndex() &&
           left.blockHash() == right.blockHash() &&
           left.previousHash() == right.previousHash() &&
           left.round() == right.round() &&
           left.decision() == right.decision();
}

bool VotePool::isMinimallyValidVote(
    const ValidatorVoteRecord& vote,
    const crypto::CryptoPolicy& policy,
    const crypto::SignatureProvider& provider
) {
    if (vote.validatorAddress().empty() ||
        vote.blockIndex() == 0 ||
        vote.blockHash().empty() ||
        vote.previousHash().empty() ||
        vote.round() == 0 ||
        vote.decision() == ValidatorVoteDecision::UNKNOWN ||
        vote.createdAt() <= 0) {
        return false;
    }

    // Verify the cryptographic signature before admitting the vote into the pool.
    // This prevents fabricated votes from poisoning the conflict-detection and
    // quorum-building state with unverified validator identities.
    // Development mode skips crypto verification so localnet/test nodes can use
    // deterministic key material without real BLS signatures.
    if (policy.developmentMode()) {
        return true;
    }
    return vote.verify(policy, provider);
}

QuorumCertificateBuildResult QuorumAssembly::tryBuildCertificate(
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
) {
    return QuorumCertificateBuilder::buildFromVotes(
        blockIndex,
        blockHash,
        previousHash,
        round,
        votePool.votesForBlock(blockIndex, blockHash, round),
        validatorRegistry,
        policy,
        provider,
        thresholdNumerator,
        thresholdDenominator
    );
}

/**
 * Prunes obsolete votes and conflict records from the pool based on the current chain height.
 * Releases memory for votes referencing older blocks that have already been finalized
 * or discarded by the network.
 */
void VotePool::prune(std::uint64_t currentHeight) {
    for (auto it = m_votesByBlock.begin(); it != m_votesByBlock.end(); ) {
        if (it->first.blockIndex() < currentHeight) {
            it = m_votesByBlock.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = m_voteByValidatorHeightRound.begin(); it != m_voteByValidatorHeightRound.end(); ) {
        if (it->second.blockIndex() < currentHeight) {
            m_voteIds.erase(it->second.deterministicId());
            it = m_voteByValidatorHeightRound.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = m_conflictingVotes.begin(); it != m_conflictingVotes.end(); ) {
        if (it->blockIndex() < currentHeight) {
            it = m_conflictingVotes.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = m_conflictingVoteCounts.begin(); it != m_conflictingVoteCounts.end(); ) {
        const std::string& key = it->first;
        std::size_t firstHash = key.find('#');
        if (firstHash != std::string::npos) {
            std::size_t secondHash = key.find('#', firstHash + 1);
            if (secondHash != std::string::npos) {
                std::string heightStr = key.substr(firstHash + 1, secondHash - firstHash - 1);
                try {
                    std::uint64_t height = std::stoull(heightStr);
                    if (height < currentHeight) {
                        it = m_conflictingVoteCounts.erase(it);
                        continue;
                    }
                } catch (...) {}
            }
        }
        ++it;
    }
}

} // namespace nodo::consensus
