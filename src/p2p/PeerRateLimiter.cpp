#include "p2p/PeerRateLimiter.hpp"

namespace nodo::p2p {

PeerRateLimiter::PeerRateLimiter()
    : m_maxMessagesPerWindow(DEFAULT_RATE_LIMIT_MESSAGES),
      m_windowSeconds(DEFAULT_RATE_LIMIT_WINDOW_SECONDS) {}

PeerRateLimiter::PeerRateLimiter(
    std::uint32_t maxMessagesPerWindow,
    std::uint64_t windowSeconds
)
    : m_maxMessagesPerWindow(maxMessagesPerWindow),
      m_windowSeconds(windowSeconds) {}

void PeerRateLimiter::advanceWindowIfExpired(PeerWindow& window, std::int64_t now) const {
    const std::int64_t windowEnd =
        window.windowStart + static_cast<std::int64_t>(m_windowSeconds);
    if (now >= windowEnd) {
        window.windowStart = now;
        window.count = 0;
    }
}

bool PeerRateLimiter::shouldAllow(const std::string& nodeId, std::int64_t now) {
    auto it = m_windows.find(nodeId);
    if (it == m_windows.end()) {
        m_windows[nodeId] = PeerWindow{now, 0};
        it = m_windows.find(nodeId);
    }

    advanceWindowIfExpired(it->second, now);

    if (it->second.count >= m_maxMessagesPerWindow) {
        return false;
    }

    it->second.count++;
    return true;
}

void PeerRateLimiter::recordInvalidMessage(const std::string& nodeId, std::int64_t now) {
    // Invalid messages are more dangerous than normal traffic, so they consume
    // extra budget in the same fixed window.
    shouldAllow(nodeId, now);
    shouldAllow(nodeId, now);
}

std::uint32_t PeerRateLimiter::messageCount(const std::string& nodeId, std::int64_t now) const {
    auto it = m_windows.find(nodeId);
    if (it == m_windows.end()) {
        return 0;
    }
    PeerWindow window = it->second;
    advanceWindowIfExpired(window, now);
    return window.count;
}

std::uint32_t PeerRateLimiter::maxMessagesPerWindow() const { return m_maxMessagesPerWindow; }
std::uint64_t PeerRateLimiter::windowSeconds() const { return m_windowSeconds; }

} // namespace nodo::p2p
