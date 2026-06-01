#include "consensus/ConsensusRoundManager.hpp"

#include <sstream>
#include <utility>

namespace nodo::consensus {

ConsensusRoundState::ConsensusRoundState()
    : m_height(0),
      m_round(0),
      m_proposerAddress(""),
      m_roundStartedAt(0) {}

ConsensusRoundState::ConsensusRoundState(
    std::uint64_t height,
    std::uint64_t round,
    std::string proposerAddress,
    std::int64_t roundStartedAt
)
    : m_height(height),
      m_round(round),
      m_proposerAddress(std::move(proposerAddress)),
      m_roundStartedAt(roundStartedAt) {}

std::uint64_t ConsensusRoundState::height() const { return m_height; }
std::uint64_t ConsensusRoundState::round() const { return m_round; }
const std::string& ConsensusRoundState::proposerAddress() const { return m_proposerAddress; }
std::int64_t ConsensusRoundState::roundStartedAt() const { return m_roundStartedAt; }

bool ConsensusRoundState::isValid() const {
    return m_roundStartedAt > 0;
}

std::string ConsensusRoundState::serialize() const {
    std::ostringstream oss;
    oss << "ConsensusRoundState{"
        << "height=" << m_height
        << ";round=" << m_round
        << ";proposer=" << m_proposerAddress
        << ";startedAt=" << m_roundStartedAt
        << "}";
    return oss.str();
}

ConsensusRoundManager::ConsensusRoundManager()
    : m_state(),
      m_voteCollector(),
      m_timeout() {}

void ConsensusRoundManager::advanceToHeight(
    std::uint64_t height,
    std::uint64_t round,
    const std::string& proposerAddress,
    std::int64_t now,
    std::uint64_t timeoutSeconds
) {
    m_state = ConsensusRoundState(height, round, proposerAddress, now);
    m_voteCollector = NetworkVoteCollector(height, round);
    m_timeout = RoundTimeout(height, round, now, timeoutSeconds);
}

void ConsensusRoundManager::advanceRound(
    std::uint64_t newRound,
    const std::string& newProposerAddress,
    std::int64_t now,
    std::uint64_t timeoutSeconds
) {
    advanceToHeight(m_state.height(), newRound, newProposerAddress, now, timeoutSeconds);
}

bool ConsensusRoundManager::isCurrentRound(
    std::uint64_t height,
    std::uint64_t round
) const {
    return m_state.height() == height && m_state.round() == round;
}

VoteCollectResult ConsensusRoundManager::submitVote(const ValidatorVoteRecord& vote) {
    return m_voteCollector.submitNetworkVote(vote);
}

bool ConsensusRoundManager::isTimeoutExpired(std::int64_t now) const {
    return m_timeout.isValid() && m_timeout.hasExpired(now);
}

const ConsensusRoundState& ConsensusRoundManager::currentState() const { return m_state; }
const NetworkVoteCollector& ConsensusRoundManager::voteCollector() const { return m_voteCollector; }
const RoundTimeout& ConsensusRoundManager::roundTimeout() const { return m_timeout; }

} // namespace nodo::consensus
