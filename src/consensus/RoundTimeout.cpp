#include "consensus/RoundTimeout.hpp"

#include <limits>
#include <stdexcept>

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
    const auto delta = static_cast<std::int64_t>(m_timeoutSeconds);
    if (delta > 0 && m_startedAt > std::numeric_limits<std::int64_t>::max() - delta) {
        return std::numeric_limits<std::int64_t>::max();
    }
    return m_startedAt + delta;
}

bool RoundTimeout::isValid() const {
    return m_startedAt > 0 && m_timeoutSeconds > 0;
}

} // namespace nodo::consensus
