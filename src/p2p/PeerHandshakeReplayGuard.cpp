#include "p2p/PeerHandshakeReplayGuard.hpp"

#include "crypto/Hex.hpp"

#include <openssl/rand.h>

#include <array>
#include <limits>
#include <stdexcept>

namespace nodo::p2p {

PeerHandshakeReplayGuard::PeerHandshakeReplayGuard(std::size_t maxEntries)
    : m_maxEntries(maxEntries),
      m_outstandingByPeer(),
      m_consumedChallenges() {
    if (m_maxEntries == 0) {
        throw std::invalid_argument(
            "Peer handshake replay cache capacity must be positive."
        );
    }
}

std::optional<std::string> PeerHandshakeReplayGuard::issueChallenge(
    const std::string& peerNodeId,
    std::int64_t now,
    std::uint32_t ttlSeconds
) {
    if (!isValidPeerNodeId(peerNodeId) || now <= 0 || ttlSeconds == 0 ||
        now > std::numeric_limits<std::int64_t>::max() - ttlSeconds) {
        return std::nullopt;
    }

    prune(now);
    const bool replacesOutstanding =
        m_outstandingByPeer.find(peerNodeId) != m_outstandingByPeer.end();
    if (!replacesOutstanding &&
        m_outstandingByPeer.size() + m_consumedChallenges.size() >=
            m_maxEntries) {
        return std::nullopt;
    }

    std::array<unsigned char, NONCE_BYTES> bytes = {};
    for (int attempt = 0; attempt < 8; ++attempt) {
        if (RAND_bytes(bytes.data(), static_cast<int>(bytes.size())) != 1) {
            return std::nullopt;
        }
        const std::string nonce =
            crypto::hexEncode(bytes.data(), bytes.size());

        bool collision = false;
        for (const auto& [_, entry] : m_outstandingByPeer) {
            if (entry.nonce == nonce) {
                collision = true;
                break;
            }
        }
        if (m_consumedChallenges.find(challengeKey(peerNodeId, nonce)) !=
                m_consumedChallenges.end()) {
            collision = true;
        }
        if (collision) continue;

        m_outstandingByPeer[peerNodeId] = ChallengeEntry{
            nonce,
            now + static_cast<std::int64_t>(ttlSeconds)
        };
        return nonce;
    }

    return std::nullopt;
}

std::optional<std::string> PeerHandshakeReplayGuard::outstandingChallenge(
    const std::string& peerNodeId,
    std::int64_t now
) {
    if (!isValidPeerNodeId(peerNodeId) || now <= 0) {
        return std::nullopt;
    }
    prune(now);
    const auto found = m_outstandingByPeer.find(peerNodeId);
    if (found == m_outstandingByPeer.end()) {
        return std::nullopt;
    }
    return found->second.nonce;
}

bool PeerHandshakeReplayGuard::consumeChallenge(
    const std::string& peerNodeId,
    const std::string& nonce,
    std::int64_t now
) {
    if (!isValidPeerNodeId(peerNodeId) || !isValidNonce(nonce) || now <= 0) {
        return false;
    }
    prune(now);
    const auto found = m_outstandingByPeer.find(peerNodeId);
    if (found == m_outstandingByPeer.end() || found->second.nonce != nonce) {
        return false;
    }
    if (m_outstandingByPeer.size() + m_consumedChallenges.size() >
        m_maxEntries) {
        return false;
    }
    m_consumedChallenges[challengeKey(peerNodeId, nonce)] =
        found->second.expiresAt;
    m_outstandingByPeer.erase(found);
    return true;
}

bool PeerHandshakeReplayGuard::discardChallenge(
    const std::string& peerNodeId,
    const std::string& nonce
) {
    if (!isValidPeerNodeId(peerNodeId) || !isValidNonce(nonce)) {
        return false;
    }
    const auto found = m_outstandingByPeer.find(peerNodeId);
    if (found == m_outstandingByPeer.end() || found->second.nonce != nonce) {
        return false;
    }
    m_outstandingByPeer.erase(found);
    return true;
}

bool PeerHandshakeReplayGuard::wasChallengeConsumed(
    const std::string& peerNodeId,
    const std::string& nonce,
    std::int64_t now
) {
    if (!isValidPeerNodeId(peerNodeId) || !isValidNonce(nonce) || now <= 0) {
        return false;
    }
    prune(now);
    return m_consumedChallenges.find(challengeKey(peerNodeId, nonce)) !=
           m_consumedChallenges.end();
}

void PeerHandshakeReplayGuard::prune(std::int64_t now) {
    if (now <= 0) return;

    for (auto iterator = m_outstandingByPeer.begin();
         iterator != m_outstandingByPeer.end();) {
        if (now > iterator->second.expiresAt) {
            iterator = m_outstandingByPeer.erase(iterator);
        } else {
            ++iterator;
        }
    }

    for (auto iterator = m_consumedChallenges.begin();
         iterator != m_consumedChallenges.end();) {
        if (now > iterator->second) {
            iterator = m_consumedChallenges.erase(iterator);
        } else {
            ++iterator;
        }
    }
}

std::size_t PeerHandshakeReplayGuard::outstandingCount() const {
    return m_outstandingByPeer.size();
}

std::size_t PeerHandshakeReplayGuard::consumedCount() const {
    return m_consumedChallenges.size();
}

bool PeerHandshakeReplayGuard::isValidNonce(const std::string& nonce) {
    return crypto::hasHexByteSize(nonce, NONCE_BYTES);
}

bool PeerHandshakeReplayGuard::isValidPeerNodeId(
    const std::string& peerNodeId
) {
    return !peerNodeId.empty() && peerNodeId.size() <= 160;
}

std::string PeerHandshakeReplayGuard::challengeKey(
    const std::string& peerNodeId,
    const std::string& nonce
) {
    return std::to_string(peerNodeId.size()) + ":" + peerNodeId + ":" + nonce;
}

} // namespace nodo::p2p
