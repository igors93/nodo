#ifndef NODO_CONSENSUS_ROUND_TIMEOUT_HPP
#define NODO_CONSENSUS_ROUND_TIMEOUT_HPP

#include <cstdint>

namespace nodo::consensus {

constexpr std::uint64_t DEFAULT_ROUND_TIMEOUT_SECONDS = 30;

/*
 * RoundTimeout tracks when a consensus round has exceeded its time budget.
 *
 * Security principle:
 * A round that never expires allows a faulty proposer to halt the network.
 * The timeout forces a round increment so that an honest proposer gets a
 * chance. Timeout values must be large enough to allow honest block production
 * under normal network conditions.
 */
class RoundTimeout {
public:
    RoundTimeout();

    RoundTimeout(
        std::uint64_t height,
        std::uint64_t round,
        std::int64_t startedAt,
        std::uint64_t timeoutSeconds = DEFAULT_ROUND_TIMEOUT_SECONDS
    );

    std::uint64_t height() const;
    std::uint64_t round() const;
    std::int64_t startedAt() const;
    std::uint64_t timeoutSeconds() const;

    bool hasExpired(std::int64_t now) const;
    std::int64_t expiresAt() const;

    bool isValid() const;

private:
    std::uint64_t m_height;
    std::uint64_t m_round;
    std::int64_t m_startedAt;
    std::uint64_t m_timeoutSeconds;
};

} // namespace nodo::consensus

#endif
