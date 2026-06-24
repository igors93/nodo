#include "consensus/NetworkVoteCollector.hpp"

#include <utility>

namespace nodo::consensus {

std::string voteCollectStatusToString(VoteCollectStatus status) {
    switch (status) {
        case VoteCollectStatus::ACCEPTED:             return "ACCEPTED";
        case VoteCollectStatus::REJECTED_STALE_ROUND: return "REJECTED_STALE_ROUND";
        case VoteCollectStatus::REJECTED_REPLAY:      return "REJECTED_REPLAY";
        case VoteCollectStatus::REJECTED_CONFLICTING: return "REJECTED_CONFLICTING";
        case VoteCollectStatus::REJECTED_INVALID:      return "REJECTED_INVALID";
        case VoteCollectStatus::REJECTED_NOT_ELIGIBLE: return "REJECTED_NOT_ELIGIBLE";
        case VoteCollectStatus::DOUBLE_VOTE_DETECTED:  return "DOUBLE_VOTE_DETECTED";
        default:                                       return "UNKNOWN";
    }
}

VoteCollectResult::VoteCollectResult()
    : m_status(VoteCollectStatus::REJECTED_INVALID),
      m_reason("") {}

VoteCollectResult::VoteCollectResult(VoteCollectStatus status, std::string reason)
    : m_status(status),
      m_reason(std::move(reason)) {}

VoteCollectStatus VoteCollectResult::status() const { return m_status; }
const std::string& VoteCollectResult::reason() const { return m_reason; }
bool VoteCollectResult::accepted() const { return m_status == VoteCollectStatus::ACCEPTED; }

NetworkVoteCollector::NetworkVoteCollector()
    : m_pool(),
      m_currentHeight(0),
      m_currentRound(0),
      m_eligibleValidators() {}

NetworkVoteCollector::NetworkVoteCollector(
    std::uint64_t currentHeight,
    std::uint64_t currentRound
)
    : m_pool(),
      m_currentHeight(currentHeight),
      m_currentRound(currentRound),
      m_eligibleValidators() {}

void NetworkVoteCollector::setCurrentRound(std::uint64_t height, std::uint64_t round) {
    m_currentHeight = height;
    m_currentRound = round;
}

std::uint64_t NetworkVoteCollector::currentHeight() const { return m_currentHeight; }
std::uint64_t NetworkVoteCollector::currentRound() const { return m_currentRound; }

void NetworkVoteCollector::setEligibleValidators(
    std::vector<std::string> eligibleAddresses
) {
    m_eligibleValidators = std::move(eligibleAddresses);
}

bool NetworkVoteCollector::hasDoubleVote(const ValidatorVoteRecord& vote) const {
    return m_pool.hasConflictingVote(
        vote.validatorAddress(),
        vote.blockIndex(),
        vote.round()
    );
}

VoteCollectResult NetworkVoteCollector::submitNetworkVote(
    const ValidatorVoteRecord& vote,
    const crypto::CryptoPolicy& policy,
    const crypto::SignatureProvider& provider
) {
    if (!m_eligibleValidators.empty()) {
        const std::string& addr = vote.validatorAddress();
        bool eligible = false;
        for (const auto& e : m_eligibleValidators) {
            if (e == addr) { eligible = true; break; }
        }
        if (!eligible) {
            return VoteCollectResult(
                VoteCollectStatus::REJECTED_NOT_ELIGIBLE,
                "Validator " + addr + " is not eligible to vote."
            );
        }
    }

    if (vote.blockIndex() < m_currentHeight) {
        return VoteCollectResult(
            VoteCollectStatus::REJECTED_STALE_ROUND,
            "Vote block index " + std::to_string(vote.blockIndex()) +
            " is below current height " + std::to_string(m_currentHeight)
        );
    }
    if (vote.blockIndex() == m_currentHeight && vote.round() < m_currentRound) {
        return VoteCollectResult(
            VoteCollectStatus::REJECTED_STALE_ROUND,
            "Vote round " + std::to_string(vote.round()) +
            " is below current round " + std::to_string(m_currentRound) +
            " for height " + std::to_string(m_currentHeight)
        );
    }
    if (vote.blockIndex() > m_currentHeight) {
        return VoteCollectResult(
            VoteCollectStatus::REJECTED_INVALID,
            "Vote block index " + std::to_string(vote.blockIndex()) +
            " is above current height " + std::to_string(m_currentHeight)
        );
    }
    if (vote.round() > m_currentRound) {
        return VoteCollectResult(
            VoteCollectStatus::REJECTED_INVALID,
            "Vote round " + std::to_string(vote.round()) +
            " is above current round " + std::to_string(m_currentRound) +
            " for height " + std::to_string(m_currentHeight)
        );
    }

    const VotePoolResult poolResult = m_pool.submitVote(vote, policy, provider);
    if (poolResult.duplicate()) {
        return VoteCollectResult(
            VoteCollectStatus::REJECTED_REPLAY,
            "Duplicate vote detected: " + poolResult.reason()
        );
    }
    if (poolResult.conflicting()) {
        return VoteCollectResult(
            VoteCollectStatus::REJECTED_CONFLICTING,
            "Conflicting vote detected: " + poolResult.reason()
        );
    }
    if (!poolResult.accepted()) {
        return VoteCollectResult(
            VoteCollectStatus::REJECTED_INVALID,
            "Vote rejected by pool: " + poolResult.reason()
        );
    }
    return VoteCollectResult(VoteCollectStatus::ACCEPTED, "Vote accepted.");
}

const VotePool& NetworkVoteCollector::votePool() const { return m_pool; }

std::size_t NetworkVoteCollector::acceptedVoteCount() const {
    return m_pool.totalVoteCount();
}

} // namespace nodo::consensus
