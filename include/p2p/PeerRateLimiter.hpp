#ifndef NODO_P2P_PEER_RATE_LIMITER_HPP
#define NODO_P2P_PEER_RATE_LIMITER_HPP

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>

namespace nodo::p2p {

constexpr std::uint32_t DEFAULT_RATE_LIMIT_MESSAGES = 100;
constexpr std::uint64_t DEFAULT_RATE_LIMIT_WINDOW_SECONDS = 60;
constexpr std::size_t MAX_TRACKED_RATE_LIMIT_PEERS = 4096;

/*
 * PeerRateLimiter enforces a per-peer message rate cap.
 *
 * Security principle:
 * A flooded peer can exhaust CPU and memory resources and disrupt consensus.
 * This limiter uses a fixed-window counter per peer. When a peer exceeds the
 * configured message threshold within the window, further messages are blocked
 * until the window resets. The design is simple and stateless across restarts.
 */
class PeerRateLimiter {
public:
    PeerRateLimiter();

    PeerRateLimiter(
        std::uint32_t maxMessagesPerWindow,
        std::uint64_t windowSeconds
    );

    bool shouldAllow(const std::string& nodeId, std::int64_t now);

    void recordInvalidMessage(const std::string& nodeId, std::int64_t now);

    std::uint32_t messageCount(const std::string& nodeId, std::int64_t now) const;

    std::uint32_t maxMessagesPerWindow() const;
    std::uint64_t windowSeconds() const;

private:
    struct PeerWindow {
        std::int64_t windowStart;
        std::uint32_t count;
    };

    std::map<std::string, PeerWindow> m_windows;
    std::uint32_t m_maxMessagesPerWindow;
    std::uint64_t m_windowSeconds;

    void advanceWindowIfExpired(PeerWindow& window, std::int64_t now) const;
    void pruneExpiredWindows(std::int64_t now);
};

} // namespace nodo::p2p

#endif
