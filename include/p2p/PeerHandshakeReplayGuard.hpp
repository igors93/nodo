#ifndef NODO_P2P_PEER_HANDSHAKE_REPLAY_GUARD_HPP
#define NODO_P2P_PEER_HANDSHAKE_REPLAY_GUARD_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

namespace nodo::p2p {

class PeerHandshakeReplayGuard {
public:
    static constexpr std::size_t DEFAULT_MAX_ENTRIES = 4096;
    static constexpr std::size_t NONCE_BYTES = 32;

    explicit PeerHandshakeReplayGuard(
        std::size_t maxEntries = DEFAULT_MAX_ENTRIES
    );

    std::optional<std::string> issueChallenge(
        const std::string& peerNodeId,
        std::int64_t now,
        std::uint32_t ttlSeconds
    );

    std::optional<std::string> outstandingChallenge(
        const std::string& peerNodeId,
        std::int64_t now
    );

    bool consumeChallenge(
        const std::string& peerNodeId,
        const std::string& nonce,
        std::int64_t now
    );

    bool discardChallenge(
        const std::string& peerNodeId,
        const std::string& nonce
    );

    bool wasChallengeConsumed(
        const std::string& peerNodeId,
        const std::string& nonce,
        std::int64_t now
    );

    void prune(std::int64_t now);

    std::size_t outstandingCount() const;
    std::size_t consumedCount() const;

    static bool isValidNonce(const std::string& nonce);

private:
    struct ChallengeEntry {
        std::string nonce;
        std::int64_t expiresAt;
    };

    static bool isValidPeerNodeId(const std::string& peerNodeId);
    static std::string challengeKey(
        const std::string& peerNodeId,
        const std::string& nonce
    );

    std::size_t m_maxEntries;
    std::unordered_map<std::string, ChallengeEntry> m_outstandingByPeer;
    std::unordered_map<std::string, std::int64_t> m_consumedChallenges;
};

} // namespace nodo::p2p

#endif
