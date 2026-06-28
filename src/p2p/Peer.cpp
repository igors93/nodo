#include "p2p/Peer.hpp"

#include "crypto/hash.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <sstream>
#include <utility>

namespace nodo::p2p {

namespace {

bool isSafePeerScalar(const std::string& value, std::size_t maxSize) {
    if (value.empty() || value.size() > maxSize) {
        return false;
    }

    for (const char character : value) {
        const bool allowed =
            (character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') ||
            character == '_' || character == '-' || character == '.' ||
            character == ':' || character == '/';

        if (!allowed) {
            return false;
        }
    }

    return true;
}

} // namespace

PeerEndpoint::PeerEndpoint()
    : m_host(""),
      m_port(0) {}

PeerEndpoint::PeerEndpoint(std::string host, std::uint16_t port)
    : m_host(std::move(host)),
      m_port(port) {}

const std::string& PeerEndpoint::host() const { return m_host; }
std::uint16_t PeerEndpoint::port() const { return m_port; }

bool PeerEndpoint::isValid() const {
    return isSafePeerScalar(m_host, 255) && m_port != 0;
}

std::string PeerEndpoint::serialize() const {
    std::ostringstream output;
    output << "PeerEndpoint{host=" << m_host << ";port=" << m_port << "}";
    return output.str();
}

PeerMetadata::PeerMetadata()
    : m_nodeId(""),
      m_endpoint(),
      m_publicKeyFingerprint(""),
      m_firstSeenAt(0),
      m_lastSeenAt(0),
      m_score(0),
      m_quarantined(false) {}

PeerMetadata::PeerMetadata(
    std::string nodeId,
    PeerEndpoint endpoint,
    std::string publicKeyFingerprint,
    std::int64_t firstSeenAt,
    std::int64_t lastSeenAt,
    std::int32_t score,
    bool quarantined
) : m_nodeId(std::move(nodeId)),
    m_endpoint(std::move(endpoint)),
    m_publicKeyFingerprint(std::move(publicKeyFingerprint)),
    m_firstSeenAt(firstSeenAt),
    m_lastSeenAt(lastSeenAt),
    m_score(score),
    m_quarantined(quarantined) {}

const std::string& PeerMetadata::nodeId() const { return m_nodeId; }
const PeerEndpoint& PeerMetadata::endpoint() const { return m_endpoint; }
const std::string& PeerMetadata::publicKeyFingerprint() const { return m_publicKeyFingerprint; }
std::int64_t PeerMetadata::firstSeenAt() const { return m_firstSeenAt; }
std::int64_t PeerMetadata::lastSeenAt() const { return m_lastSeenAt; }
std::int32_t PeerMetadata::score() const { return m_score; }
bool PeerMetadata::quarantined() const { return m_quarantined; }

std::string PeerMetadata::identityKey() const {
    std::string canonicalFingerprint = m_publicKeyFingerprint;
    std::transform(
        canonicalFingerprint.begin(),
        canonicalFingerprint.end(),
        canonicalFingerprint.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        }
    );
    char digest[NODO_HASH_BUFFER_SIZE] = {};
    const std::string payload =
        "NODO_PEER_IDENTITY_V1|" + canonicalFingerprint;
    nodo_hash_string(payload.c_str(), digest, sizeof(digest));
    return "peer-key-" + std::string(digest);
}

PeerMetadata PeerMetadata::withHeartbeat(std::int64_t lastSeenAt) const {
    return PeerMetadata(
        m_nodeId,
        m_endpoint,
        m_publicKeyFingerprint,
        m_firstSeenAt,
        lastSeenAt,
        m_score,
        m_quarantined
    );
}

PeerMetadata PeerMetadata::withScoreDelta(std::int32_t delta) const {
    const std::int64_t adjusted =
        static_cast<std::int64_t>(m_score) + static_cast<std::int64_t>(delta);
    const std::int64_t bounded = std::max(
        static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min()),
        std::min(
            static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max()),
            adjusted
        )
    );
    return PeerMetadata(
        m_nodeId,
        m_endpoint,
        m_publicKeyFingerprint,
        m_firstSeenAt,
        m_lastSeenAt,
        static_cast<std::int32_t>(bounded),
        m_quarantined
    );
}

PeerMetadata PeerMetadata::quarantinedCopy() const {
    return PeerMetadata(
        m_nodeId,
        m_endpoint,
        m_publicKeyFingerprint,
        m_firstSeenAt,
        m_lastSeenAt,
        m_score,
        true
    );
}

bool PeerMetadata::isValid() const {
    return isSafePeerScalar(m_nodeId, 160) &&
           m_endpoint.isValid() &&
           isSafePeerScalar(m_publicKeyFingerprint, 160) &&
           m_firstSeenAt > 0 &&
           m_lastSeenAt >= m_firstSeenAt;
}

std::string PeerMetadata::serialize() const {
    std::ostringstream output;
    output << "PeerMetadata{"
           << "nodeId=" << m_nodeId
           << ";endpoint=" << m_endpoint.serialize()
           << ";publicKeyFingerprint=" << m_publicKeyFingerprint
           << ";firstSeenAt=" << m_firstSeenAt
           << ";lastSeenAt=" << m_lastSeenAt
           << ";score=" << m_score
           << ";quarantined=" << (m_quarantined ? "true" : "false")
           << "}";
    return output.str();
}

} // namespace nodo::p2p
