#include "p2p/PeerReconnectionPolicy.hpp"

#include <algorithm>
#include <sstream>

namespace nodo::p2p {

bool PeerReconnectionState::isReadyToRetry(std::int64_t now) const {
    if (quarantined) return false;
    if (attempts >= PeerReconnectionPolicy::MAX_ATTEMPTS) return false;
    return now >= nextRetryAt;
}

std::string PeerReconnectionState::serialize() const {
    std::ostringstream oss;
    oss << "PeerReconnectionState{"
        << "nodeId=" << nodeId
        << ";endpoint=" << endpoint
        << ";attempts=" << attempts
        << ";lastAttemptAt=" << lastAttemptAt
        << ";nextRetryAt=" << nextRetryAt
        << ";quarantined=" << (quarantined ? "true" : "false")
        << "}";
    return oss.str();
}

std::int64_t PeerReconnectionPolicy::backoffDelay(std::uint32_t attempt) {
    std::int64_t delay = BASE_DELAY_SECONDS;
    for (std::uint32_t i = 0; i < attempt; ++i) {
        delay *= 2;
        if (delay >= MAX_DELAY_SECONDS) {
            delay = MAX_DELAY_SECONDS;
            break;
        }
    }
    return delay;
}

void PeerReconnectionPolicy::recordDisconnect(
    const std::string& nodeId,
    const std::string& endpoint,
    std::int64_t       now
) {
    if (m_states.count(nodeId) == 0) {
        PeerReconnectionState state{};
        state.nodeId         = nodeId;
        state.endpoint       = endpoint;
        state.attempts       = 0;
        state.lastAttemptAt  = 0;
        state.nextRetryAt    = now + BASE_DELAY_SECONDS;
        state.quarantined    = false;
        m_states[nodeId] = std::move(state);
    } else {
        // Reset only if not quarantined.
        auto& s = m_states[nodeId];
        if (!s.quarantined) {
            s.endpoint    = endpoint;
            s.nextRetryAt = now + BASE_DELAY_SECONDS;
        }
    }
}

void PeerReconnectionPolicy::recordAttempt(
    const std::string& nodeId,
    std::int64_t       now
) {
    auto it = m_states.find(nodeId);
    if (it == m_states.end()) return;

    auto& s = it->second;
    s.attempts++;
    s.lastAttemptAt = now;
    s.nextRetryAt   = now + backoffDelay(s.attempts);
}

void PeerReconnectionPolicy::recordSuccess(const std::string& nodeId) {
    m_states.erase(nodeId);
}

void PeerReconnectionPolicy::recordFailure(
    const std::string& nodeId,
    std::int64_t       now
) {
    auto it = m_states.find(nodeId);
    if (it == m_states.end()) return;

    auto& s = it->second;
    s.nextRetryAt = now + backoffDelay(s.attempts);
}

void PeerReconnectionPolicy::quarantine(
    const std::string& nodeId,
    const std::string& reason,
    std::int64_t       now
) {
    auto it = m_states.find(nodeId);
    if (it == m_states.end()) return;

    auto& s = it->second;
    s.quarantined       = true;
    s.quarantineReason  = reason;
    s.nextRetryAt       = now + QUARANTINE_COOLDOWN;
}

void PeerReconnectionPolicy::lift(const std::string& nodeId) {
    auto it = m_states.find(nodeId);
    if (it == m_states.end()) return;

    auto& s = it->second;
    s.quarantined      = false;
    s.quarantineReason = "";
    s.attempts         = 0;
}

std::vector<PeerReconnectionState> PeerReconnectionPolicy::candidatesForReconnect(
    std::int64_t now
) const {
    std::vector<PeerReconnectionState> result;
    for (const auto& [id, state] : m_states) {
        if (state.isReadyToRetry(now)) {
            result.push_back(state);
        }
    }
    std::sort(
        result.begin(), result.end(),
        [](const PeerReconnectionState& a, const PeerReconnectionState& b) {
            return a.nextRetryAt < b.nextRetryAt;
        }
    );
    return result;
}

bool PeerReconnectionPolicy::isTracked(const std::string& nodeId) const {
    return m_states.count(nodeId) > 0;
}

bool PeerReconnectionPolicy::isQuarantined(const std::string& nodeId) const {
    auto it = m_states.find(nodeId);
    return it != m_states.end() && it->second.quarantined;
}

const PeerReconnectionState* PeerReconnectionPolicy::state(
    const std::string& nodeId
) const {
    auto it = m_states.find(nodeId);
    return it != m_states.end() ? &it->second : nullptr;
}

std::size_t PeerReconnectionPolicy::trackedCount() const {
    return m_states.size();
}

std::size_t PeerReconnectionPolicy::quarantineCount() const {
    std::size_t count = 0;
    for (const auto& [id, s] : m_states) {
        if (s.quarantined) ++count;
    }
    return count;
}

std::string PeerReconnectionPolicy::serialize() const {
    std::ostringstream oss;
    oss << "PeerReconnectionPolicy{"
        << "tracked=" << m_states.size()
        << ";quarantined=" << quarantineCount()
        << "}";
    return oss.str();
}

// ---- PeerExchangeService --------------------------------------------------

std::vector<PeerExchangeEntry> PeerExchangeService::buildPayload(
    const std::vector<PeerMetadata>& activePeers,
    std::size_t                      maxPeers
) {
    std::vector<PeerExchangeEntry> entries;
    entries.reserve(std::min(activePeers.size(), maxPeers));

    for (const auto& peer : activePeers) {
        if (entries.size() >= maxPeers) break;
        entries.push_back({
            peer.nodeId(),
            peer.endpoint().serialize(),
            peer.publicKeyFingerprint()
        });
    }
    return entries;
}

void PeerExchangeService::mergeInto(
    const std::vector<PeerExchangeEntry>& entries,
    PeerReconnectionPolicy&               policy,
    std::int64_t                          now
) {
    for (const auto& entry : entries) {
        if (!policy.isTracked(entry.nodeId) &&
            !policy.isQuarantined(entry.nodeId)) {
            policy.recordDisconnect(entry.nodeId, entry.endpoint, now);
        }
    }
}

std::string PeerExchangeService::serializePayload(
    const std::vector<PeerExchangeEntry>& entries
) {
    std::ostringstream oss;
    oss << "NODO_PEER_EXCHANGE_V1{"
        << "count=" << entries.size()
        << ";peers=[";
    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "{nodeId=" << entries[i].nodeId
            << ";endpoint=" << entries[i].endpoint
            << ";fp=" << entries[i].fingerprint << "}";
    }
    oss << "]}";
    return oss.str();
}

std::vector<PeerExchangeEntry> PeerExchangeService::deserializePayload(
    const std::string& /*serialized*/
) {
    // Minimal stub: full codec is defined in serialization layer.
    // Returns empty vector on unsupported format.
    return {};
}

} // namespace nodo::p2p
