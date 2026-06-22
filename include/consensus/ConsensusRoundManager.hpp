#ifndef NODO_CONSENSUS_CONSENSUS_ROUND_MANAGER_HPP
#define NODO_CONSENSUS_CONSENSUS_ROUND_MANAGER_HPP

#include "consensus/NetworkVoteCollector.hpp"
#include "consensus/RoundTimeout.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/SignatureProvider.hpp"

#include <cstdint>
#include <string>

namespace nodo::consensus {

class ConsensusRoundState {
public:
    ConsensusRoundState();

    ConsensusRoundState(
        std::uint64_t height,
        std::uint64_t round,
        std::string proposerAddress,
        std::int64_t roundStartedAt
    );

    std::uint64_t height() const;
    std::uint64_t round() const;
    const std::string& proposerAddress() const;
    std::int64_t roundStartedAt() const;

    bool isValid() const;
    std::string serialize() const;

private:
    std::uint64_t m_height;
    std::uint64_t m_round;
    std::string m_proposerAddress;
    std::int64_t m_roundStartedAt;
};

/*
 * ConsensusRoundManager coordinates the active consensus round.
 *
 * Security principle:
 * Round state is the source of truth for which votes are current. All vote
 * admission goes through this manager so that stale-round and replay checks
 * happen in a single place. The manager does not own the transport layer —
 * it receives votes from the network and returns decisions. TCP/gossip are
 * irrelevant here.
 */
class ConsensusRoundManager {
public:
    ConsensusRoundManager();

    void advanceToHeight(
        std::uint64_t height,
        std::uint64_t round,
        const std::string& proposerAddress,
        std::int64_t now,
        std::uint64_t timeoutSeconds = DEFAULT_ROUND_TIMEOUT_SECONDS
    );

    void advanceRound(
        std::uint64_t newRound,
        const std::string& newProposerAddress,
        std::int64_t now,
        std::uint64_t timeoutSeconds = DEFAULT_ROUND_TIMEOUT_SECONDS
    );

    bool isCurrentRound(std::uint64_t height, std::uint64_t round) const;

    VoteCollectResult submitVote(
        const ValidatorVoteRecord& vote,
        const crypto::CryptoPolicy& policy,
        const crypto::SignatureProvider& provider
    );

    bool isTimeoutExpired(std::int64_t now) const;

    const ConsensusRoundState& currentState() const;
    const NetworkVoteCollector& voteCollector() const;
    const RoundTimeout& roundTimeout() const;

private:
    ConsensusRoundState m_state;
    NetworkVoteCollector m_voteCollector;
    RoundTimeout m_timeout;
};

} // namespace nodo::consensus

#endif
