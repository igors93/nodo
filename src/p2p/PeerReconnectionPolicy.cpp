#include "p2p/PeerReconnectionPolicy.hpp"

#include <algorithm>
#include <limits>
#include <optional>
#include <sstream>

namespace nodo::p2p {

namespace {

constexpr std::size_t MAX_PEER_EXCHANGE_ENTRIES = 128;

bool isSafePeerExchangeScalar(
    const std::string& value,
    std::size_t maxSize
) {
    if (value.empty() || value.size() > maxSize) {
        return false;
    }

    for (const char character : value) {
        const bool allowed =
            (character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') ||
            character == '_' ||
            character == '-' ||
            character == '.' ||
            character == ':' ||
            character == '/' ||
            character == '{' ||
            character == '}' ||
            character == '=' ||
            character == ';';

        if (!allowed) {
            return false;
        }
    }

    return true;
}

std::optional<std::uint64_t> parsePeerExchangeUInt64(
    const std::string& value
) {
    if (value.empty()) {
        return std::nullopt;
    }

    for (const char character : value) {
        if (character < '0' || character > '9') {
            return std::nullopt;
        }
    }

    try {
        std::size_t parsedCharacters = 0;
        const unsigned long long parsed =
            std::stoull(value, &parsedCharacters);
        if (parsedCharacters != value.size() ||
            parsed > static_cast<unsigned long long>(
                std::numeric_limits<std::uint64_t>::max()
            )) {
            return std::nullopt;
        }
        return static_cast<std::uint64_t>(parsed);
    } catch (...) {
        return std::nullopt;
    }
}

std::string extractPeerExchangeField(
    const std::string& text,
    const std::string& key
) {
    const std::string prefix = key + "=";
    const std::size_t start = text.find(prefix);
    if (start == std::string::npos) {
        return "";
    }

    const std::size_t valueStart = start + prefix.size();
    int depth = 0;
    std::size_t index = valueStart;

    while (index < text.size()) {
        const char current = text[index];
        if (current == '{' || current == '[') {
            ++depth;
        } else if (current == '}' || current == ']') {
            if (depth == 0) {
                break;
            }
            --depth;
        } else if (current == ';' && depth == 0) {
            break;
        }
        ++index;
    }

    return text.substr(valueStart, index - valueStart);
}

std::optional<PeerEndpoint> parsePeerExchangeEndpoint(
    const std::string& serialized
) {
    if (serialized.rfind("PeerEndpoint{", 0) != 0 ||
        serialized.size() < 16 ||
        serialized.back() != '}') {
        return std::nullopt;
    }

    const std::string host =
        extractPeerExchangeField(serialized, "host");
    const std::string portText =
        extractPeerExchangeField(serialized, "port");
    const std::optional<std::uint64_t> port =
        parsePeerExchangeUInt64(portText);

    if (!isSafePeerExchangeScalar(host, 255) ||
        !port.has_value() ||
        port.value() == 0 ||
        port.value() > std::numeric_limits<std::uint16_t>::max()) {
        return std::nullopt;
    }

    PeerEndpoint endpoint(
        host,
        static_cast<std::uint16_t>(port.value())
    );

    if (!endpoint.isValid() ||
        endpoint.serialize() != serialized) {
        return std::nullopt;
    }

    return endpoint;
}

std::vector<std::string> splitPeerExchangeEntries(
    const std::string& entriesText
) {
    std::vector<std::string> chunks;
    std::size_t index = 0;

    while (index < entriesText.size()) {
        if (entriesText[index] == ',') {
            ++index;
            continue;
        }

        if (entriesText[index] != '{') {
            return {};
        }

        const std::size_t start = index;
        int depth = 0;
        bool complete = false;

        while (index < entriesText.size()) {
            const char current = entriesText[index];
            if (current == '{') {
                ++depth;
            } else if (current == '}') {
                --depth;
                if (depth == 0) {
                    ++index;
                    complete = true;
                    break;
                }
            }
            ++index;
        }

        if (!complete) {
            return {};
        }

        chunks.push_back(entriesText.substr(start, index - start));

        if (index < entriesText.size() && entriesText[index] != ',') {
            return {};
        }
    }

    return chunks;
}

} // namespace

bool PeerReconnectionState::isReadyToRetry(std::int64_t now) const {
    if (quarantined) return false;
    if (attemptInFlight) return false;
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
        << ";attemptInFlight=" << (attemptInFlight ? "true" : "false")
        << "}";
    return oss.str();
}

bool PeerReconnectionPolicy::isSafeNodeId(const std::string& nodeId) {
    if (nodeId.empty() || nodeId.size() > 160) {
        return false;
    }
    for (const char character : nodeId) {
        const bool allowed =
            (character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') ||
            character == '_' || character == '-' || character == '.' ||
            character == ':' || character == '/';
        if (!allowed) return false;
    }
    return true;
}

bool PeerReconnectionPolicy::isSafeEndpoint(const std::string& endpoint) {
    if (endpoint.empty() || endpoint.size() > 320) {
        return false;
    }
    for (const char character : endpoint) {
        const bool allowed =
            (character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') ||
            character == '_' || character == '-' || character == '.' ||
            character == ':' || character == '/' || character == '{' ||
            character == '}' || character == '=' || character == ';';
        if (!allowed) return false;
    }
    return true;
}

std::int64_t PeerReconnectionPolicy::backoffDelayForAttempt(
    std::uint32_t attempt
) {
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

void PeerReconnectionPolicy::recordCandidate(
    const std::string& nodeId,
    const std::string& endpoint,
    std::int64_t       now,
    bool               immediateRetry
) {
    if (!isSafeNodeId(nodeId) || !isSafeEndpoint(endpoint) || now <= 0) {
        return;
    }

    auto it = m_states.find(nodeId);
    if (it == m_states.end()) {
        PeerReconnectionState state{};
        state.nodeId         = nodeId;
        state.endpoint       = endpoint;
        state.attempts       = 0;
        state.lastAttemptAt  = 0;
        state.nextRetryAt    = immediateRetry ? now : now + BASE_DELAY_SECONDS;
        state.quarantined    = false;
        state.attemptInFlight = false;
        m_states[nodeId] = std::move(state);
        return;
    }

    PeerReconnectionState& state = it->second;
    if (state.quarantined) {
        return;
    }
    state.endpoint = endpoint;
    if (immediateRetry && state.nextRetryAt > now && state.attempts == 0) {
        state.nextRetryAt = now;
    }
}

void PeerReconnectionPolicy::recordDisconnect(
    const std::string& nodeId,
    const std::string& endpoint,
    std::int64_t       now
) {
    if (!isSafeNodeId(nodeId) || !isSafeEndpoint(endpoint) || now <= 0) {
        return;
    }

    auto it = m_states.find(nodeId);
    if (it == m_states.end()) {
        recordCandidate(nodeId, endpoint, now, false);
        return;
    }

    PeerReconnectionState& state = it->second;
    if (!state.quarantined) {
        state.endpoint = endpoint;
        state.attemptInFlight = false;
        if (state.nextRetryAt < now + BASE_DELAY_SECONDS) {
            state.nextRetryAt = now + BASE_DELAY_SECONDS;
        }
    }
}

void PeerReconnectionPolicy::recordAttempt(
    const std::string& nodeId,
    std::int64_t       now
) {
    auto it = m_states.find(nodeId);
    if (it == m_states.end() || now <= 0) return;

    PeerReconnectionState& state = it->second;
    if (state.quarantined) return;

    state.attempts++;
    state.lastAttemptAt    = now;
    state.nextRetryAt      = now + backoffDelayForAttempt(state.attempts);
    state.attemptInFlight  = true;
}

void PeerReconnectionPolicy::recordSuccess(const std::string& nodeId) {
    m_states.erase(nodeId);
}

void PeerReconnectionPolicy::recordFailure(
    const std::string& nodeId,
    std::int64_t       now
) {
    auto it = m_states.find(nodeId);
    if (it == m_states.end() || now <= 0) return;

    PeerReconnectionState& state = it->second;
    if (!state.attemptInFlight) {
        state.attempts++;
    }
    state.attemptInFlight = false;

    if (state.attempts >= MAX_ATTEMPTS) {
        state.quarantined = true;
        state.quarantineReason = "Maximum reconnection attempts reached.";
        state.nextRetryAt = now + QUARANTINE_COOLDOWN;
        return;
    }

    state.nextRetryAt = now + backoffDelayForAttempt(state.attempts);
}

void PeerReconnectionPolicy::quarantine(
    const std::string& nodeId,
    const std::string& reason,
    std::int64_t       now
) {
    if (!isSafeNodeId(nodeId) || now <= 0) return;

    auto it = m_states.find(nodeId);
    if (it == m_states.end()) {
        PeerReconnectionState state{};
        state.nodeId = nodeId;
        state.endpoint = "quarantined";
        state.attempts = 0;
        state.lastAttemptAt = 0;
        state.nextRetryAt = now + QUARANTINE_COOLDOWN;
        state.quarantined = true;
        state.quarantineReason = reason;
        m_states[nodeId] = std::move(state);
        return;
    }

    PeerReconnectionState& state = it->second;
    state.quarantined       = true;
    state.quarantineReason  = reason;
    state.nextRetryAt       = now + QUARANTINE_COOLDOWN;
    state.attemptInFlight   = false;
}

void PeerReconnectionPolicy::lift(
    const std::string& nodeId,
    std::int64_t now
) {
    auto it = m_states.find(nodeId);
    if (it == m_states.end()) return;

    PeerReconnectionState& state = it->second;
    state.quarantined      = false;
    state.quarantineReason = "";
    state.attempts         = 0;
    state.attemptInFlight  = false;
    if (now > 0) {
        state.nextRetryAt = now;
    }
}

std::vector<PeerReconnectionState> PeerReconnectionPolicy::candidatesForReconnect(
    std::int64_t now
) const {
    std::vector<PeerReconnectionState> result;
    for (const auto& [id, state] : m_states) {
        (void)id;
        if (state.isReadyToRetry(now)) {
            result.push_back(state);
        }
    }
    std::sort(
        result.begin(), result.end(),
        [](const PeerReconnectionState& a, const PeerReconnectionState& b) {
            if (a.nextRetryAt != b.nextRetryAt) {
                return a.nextRetryAt < b.nextRetryAt;
            }
            return a.nodeId < b.nodeId;
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
    for (const auto& [id, state] : m_states) {
        (void)id;
        if (state.quarantined) ++count;
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
            policy.recordCandidate(entry.nodeId, entry.endpoint, now, true);
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
    const std::string& serialized
) {
    if (serialized.rfind("NODO_PEER_EXCHANGE_V1{", 0) != 0 ||
        serialized.size() < 25 ||
        serialized.back() != '}') {
        return {};
    }

    const std::string countText =
        extractPeerExchangeField(serialized, "count");
    const std::optional<std::uint64_t> count =
        parsePeerExchangeUInt64(countText);

    if (!count.has_value() ||
        count.value() > MAX_PEER_EXCHANGE_ENTRIES) {
        return {};
    }

    const std::string peersMarker = ";peers=[";
    const std::size_t peersStart = serialized.find(peersMarker);
    if (peersStart == std::string::npos ||
        serialized.size() < peersStart + peersMarker.size() + 2 ||
        serialized.substr(serialized.size() - 2) != "]}") {
        return {};
    }

    const std::string entriesText =
        serialized.substr(
            peersStart + peersMarker.size(),
            serialized.size() - (peersStart + peersMarker.size()) - 2
        );

    std::vector<std::string> chunks;
    if (!entriesText.empty()) {
        chunks = splitPeerExchangeEntries(entriesText);
        if (chunks.empty()) {
            return {};
        }
    }

    if (chunks.size() != static_cast<std::size_t>(count.value())) {
        return {};
    }

    std::vector<PeerExchangeEntry> entries;
    entries.reserve(chunks.size());

    for (const std::string& chunk : chunks) {
        const std::string nodeId =
            extractPeerExchangeField(chunk, "nodeId");
        const std::string endpointText =
            extractPeerExchangeField(chunk, "endpoint");
        const std::string fingerprint =
            extractPeerExchangeField(chunk, "fp");

        const std::optional<PeerEndpoint> endpoint =
            parsePeerExchangeEndpoint(endpointText);

        if (!PeerReconnectionPolicy::isSafeNodeId(nodeId) ||
            !endpoint.has_value() ||
            !isSafePeerExchangeScalar(fingerprint, 160)) {
            return {};
        }

        entries.push_back({
            nodeId,
            endpoint->serialize(),
            fingerprint
        });
    }

    if (serializePayload(entries) != serialized) {
        return {};
    }

    return entries;
}

} // namespace nodo::p2p
