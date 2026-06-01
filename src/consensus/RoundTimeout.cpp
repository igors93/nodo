#include "consensus/RoundTimeout.hpp"

namespace nodo::consensus {

RoundTimeout::RoundTimeout()
    : m_height(0),
      m_round(0),
      m_startedAt(0),
      m_timeoutSeconds(DEFAULT_ROUND_TIMEOUT_SECONDS) {}

RoundTimeout::RoundTimeout(
    std::uint64_t height,
    std::uint64_t round,
    std::int64_t startedAt,
    std::uint64_t timeoutSeconds
)
    : m_height(height),
      m_round(round),
      m_startedAt(startedAt),
      m_timeoutSeconds(timeoutSeconds) {}

std::uint64_t RoundTimeout::height() const { return m_height; }
std::uint64_t RoundTimeout::round() const { return m_round; }
std::int64_t RoundTimeout::startedAt() const { return m_startedAt; }
std::uint64_t RoundTimeout::timeoutSeconds() const { return m_timeoutSeconds; }

bool RoundTimeout::hasExpired(std::int64_t now) const {
    return now >= expiresAt();
}

std::int64_t RoundTimeout::expiresAt() const {
    return m_startedAt + static_cast<std::int64_t>(m_timeoutSeconds);
}

bool RoundTimeout::isValid() const {
    return m_startedAt > 0 && m_timeoutSeconds > 0;
}

} // namespace nodo::consensus
