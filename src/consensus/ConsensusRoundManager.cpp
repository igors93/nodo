#include "consensus/ConsensusRoundManager.hpp"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::consensus {

ConsensusRoundState::ConsensusRoundState()
    : m_height(0),
      m_round(0),
      m_proposerAddress(""),
      m_roundStartedAt(0),
      m_lockedBlockHash(""),
      m_lockedRound(0),
      m_votedPrevote(false),
      m_votedPrecommit(false) {}

ConsensusRoundState::ConsensusRoundState(
    std::uint64_t height,
    std::uint64_t round,
    std::string proposerAddress,
    std::int64_t roundStartedAt,
    std::string lockedBlockHash,
    std::uint64_t lockedRound,
    bool votedPrevote,
    bool votedPrecommit
)
    : m_height(height),
      m_round(round),
      m_proposerAddress(std::move(proposerAddress)),
      m_roundStartedAt(roundStartedAt),
      m_lockedBlockHash(std::move(lockedBlockHash)),
      m_lockedRound(lockedRound),
      m_votedPrevote(votedPrevote),
      m_votedPrecommit(votedPrecommit) {}

std::uint64_t ConsensusRoundState::height() const { return m_height; }
std::uint64_t ConsensusRoundState::round() const { return m_round; }
const std::string& ConsensusRoundState::proposerAddress() const { return m_proposerAddress; }
std::int64_t ConsensusRoundState::roundStartedAt() const { return m_roundStartedAt; }
const std::string& ConsensusRoundState::lockedBlockHash() const { return m_lockedBlockHash; }
std::uint64_t ConsensusRoundState::lockedRound() const { return m_lockedRound; }
bool ConsensusRoundState::votedPrevote() const { return m_votedPrevote; }
bool ConsensusRoundState::votedPrecommit() const { return m_votedPrecommit; }

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
        << ";lockedBlockHash=" << m_lockedBlockHash
        << ";lockedRound=" << m_lockedRound
        << ";votedPrevote=" << (m_votedPrevote ? 1 : 0)
        << ";votedPrecommit=" << (m_votedPrecommit ? 1 : 0)
        << "}";
    return oss.str();
}

ConsensusRoundState ConsensusRoundState::deserialize(const std::string& text) {
    const std::string prefix = "ConsensusRoundState{";
    const std::string suffix = "}";

    if (text.size() < prefix.size() + suffix.size()) {
        throw std::invalid_argument("ConsensusRoundState::deserialize: input too short");
    }
    if (text.substr(0, prefix.size()) != prefix) {
        throw std::invalid_argument("ConsensusRoundState::deserialize: missing prefix");
    }
    if (text.substr(text.size() - suffix.size()) != suffix) {
        throw std::invalid_argument("ConsensusRoundState::deserialize: missing suffix");
    }

    const std::string body = text.substr(prefix.size(), text.size() - prefix.size() - suffix.size());

    auto extractField = [&](const std::string& key) -> std::string {
        const std::string search = key + "=";
        const std::size_t pos = body.find(search);
        if (pos == std::string::npos) {
            throw std::invalid_argument("ConsensusRoundState::deserialize: missing field " + key);
        }
        const std::size_t start = pos + search.size();
        const std::size_t end = body.find(';', start);
        return body.substr(start, end == std::string::npos ? std::string::npos : end - start);
    };

    const std::uint64_t height = std::stoull(extractField("height"));
    const std::uint64_t round = std::stoull(extractField("round"));
    const std::string proposer = extractField("proposer");
    const std::int64_t startedAt = std::stoll(extractField("startedAt"));
    const std::string lockedBlockHash = extractField("lockedBlockHash");
    const std::uint64_t lockedRound = std::stoull(extractField("lockedRound"));
    const bool votedPrevote = (std::stoi(extractField("votedPrevote")) != 0);
    const bool votedPrecommit = (std::stoi(extractField("votedPrecommit")) != 0);

    const ConsensusRoundState state(
        height, round, proposer, startedAt,
        lockedBlockHash, lockedRound, votedPrevote, votedPrecommit
    );
    if (!state.isValid() || state.serialize() != text) {
        throw std::invalid_argument(
            "ConsensusRoundState::deserialize: non-canonical state"
        );
    }
    return state;
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

VoteCollectResult ConsensusRoundManager::submitVote(
    const ValidatorVoteRecord& vote,
    const crypto::CryptoPolicy& policy,
    const crypto::SignatureProvider& provider
) {
    return m_voteCollector.submitNetworkVote(vote, policy, provider);
}

bool ConsensusRoundManager::isTimeoutExpired(std::int64_t now) const {
    return m_timeout.isValid() && m_timeout.hasExpired(now);
}

const ConsensusRoundState& ConsensusRoundManager::currentState() const { return m_state; }
const NetworkVoteCollector& ConsensusRoundManager::voteCollector() const { return m_voteCollector; }
const RoundTimeout& ConsensusRoundManager::roundTimeout() const { return m_timeout; }

} // namespace nodo::consensus
