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
    const bool clockMovedBackwards = now < window.windowStart;
    const std::uint64_t elapsed = clockMovedBackwards
        ? 0
        : static_cast<std::uint64_t>(now) -
            static_cast<std::uint64_t>(window.windowStart);
    if (clockMovedBackwards || elapsed >= m_windowSeconds) {
        window.windowStart = now;
        window.count = 0;
    }
}

bool PeerRateLimiter::shouldAllow(const std::string& nodeId, std::int64_t now) {
    if (nodeId.empty() || now <= 0 || m_maxMessagesPerWindow == 0 ||
        m_windowSeconds == 0) {
        return false;
    }

    auto it = m_windows.find(nodeId);
    if (it == m_windows.end()) {
        if (m_windows.size() >= MAX_TRACKED_RATE_LIMIT_PEERS) {
            pruneExpiredWindows(now);
            if (m_windows.size() >= MAX_TRACKED_RATE_LIMIT_PEERS) {
                return false;
            }
        }
        it = m_windows.emplace(nodeId, PeerWindow{now, 0}).first;
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
    if (nodeId.empty() || now <= 0 || m_windowSeconds == 0) {
        return 0;
    }
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

void PeerRateLimiter::pruneExpiredWindows(std::int64_t now) {
    for (auto it = m_windows.begin(); it != m_windows.end();) {
        PeerWindow window = it->second;
        advanceWindowIfExpired(window, now);
        if (window.count == 0) {
            it = m_windows.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace nodo::p2p
