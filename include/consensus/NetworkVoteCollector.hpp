#ifndef NODO_CONSENSUS_NETWORK_VOTE_COLLECTOR_HPP
#define NODO_CONSENSUS_NETWORK_VOTE_COLLECTOR_HPP

#include "consensus/ValidatorVoteRecord.hpp"
#include "consensus/VotePool.hpp"
#include "crypto/CryptoPolicy.hpp"
#include "crypto/SignatureProvider.hpp"

#include <cstdint>
#include <string>

namespace nodo::consensus {

enum class VoteCollectStatus {
    ACCEPTED,
    REJECTED_STALE_ROUND,
    REJECTED_REPLAY,
    REJECTED_CONFLICTING,
    REJECTED_INVALID
};

std::string voteCollectStatusToString(VoteCollectStatus status);

class VoteCollectResult {
public:
    VoteCollectResult();
    VoteCollectResult(VoteCollectStatus status, std::string reason);

    VoteCollectStatus status() const;
    const std::string& reason() const;
    bool accepted() const;

private:
    VoteCollectStatus m_status;
    std::string m_reason;
};

/*
 * NetworkVoteCollector is the network-level vote admission layer.
 *
 * Security principle:
 * Votes from stale rounds must be rejected to prevent a malicious peer from
 * replaying old votes for a different block. This collector sits above VotePool
 * and enforces round-currency before delegating to the underlying pool.
 *
 * Note: This class does NOT depend on TCP or any transport layer. It operates
 * purely on ValidatorVoteRecord values received from any source.
 */
class NetworkVoteCollector {
public:
    NetworkVoteCollector();

    NetworkVoteCollector(
        std::uint64_t currentHeight,
        std::uint64_t currentRound
    );

    void setCurrentRound(std::uint64_t height, std::uint64_t round);

    std::uint64_t currentHeight() const;
    std::uint64_t currentRound() const;

    VoteCollectResult submitNetworkVote(
        const ValidatorVoteRecord& vote,
        const crypto::CryptoPolicy& policy,
        const crypto::SignatureProvider& provider
    );

    const VotePool& votePool() const;

    std::size_t acceptedVoteCount() const;

private:
    VotePool m_pool;
    std::uint64_t m_currentHeight;
    std::uint64_t m_currentRound;
};

} // namespace nodo::consensus

#endif
