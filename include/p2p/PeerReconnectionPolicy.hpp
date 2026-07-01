#ifndef NODO_P2P_PEER_RECONNECTION_POLICY_HPP
#define NODO_P2P_PEER_RECONNECTION_POLICY_HPP

#include "p2p/Peer.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace nodo::p2p {

/*
 * PeerReconnectionState tracks retry metadata for one peer.
 *
 * Backoff schedule:
 *   first scheduled candidate : immediate or base_delay depending on caller
 *   attempt n                 : min(base_delay * 2^n, max_delay)
 *
 * A peer that has been quarantined is not eligible for reconnection until the
 * quarantine cooldown has elapsed.  attemptInFlight prevents the same candidate
 * from being returned every tick while the TCP/handshake path is still trying.
 */
struct PeerReconnectionState {
    std::string   nodeId;
    std::string   endpoint;
    std::uint32_t attempts;
    std::int64_t  lastAttemptAt;
    std::int64_t  nextRetryAt;
    bool          quarantined;
    std::string   quarantineReason;
    bool          attemptInFlight{false};

    bool isReadyToRetry(std::int64_t now) const;
    std::string serialize() const;
};

/*
 * PeerReconnectionPolicy is the deterministic peer-connectivity state machine.
 * Bootstrap peers, discovery results and disconnected known peers all enter this
 * same policy before a TCP connection attempt is made.  Callers never retry in a
 * tight loop: candidates are returned only when their backoff window opens.
 */
class PeerReconnectionPolicy {
public:
    static constexpr std::int64_t  BASE_DELAY_SECONDS  = 5;
    static constexpr std::int64_t  MAX_DELAY_SECONDS   = 300;
    static constexpr std::uint32_t MAX_ATTEMPTS        = 12;
    static constexpr std::int64_t  QUARANTINE_COOLDOWN = 3600;

    PeerReconnectionPolicy() = default;

    void recordCandidate(
        const std::string& nodeId,
        const std::string& endpoint,
        std::int64_t       now,
        bool               immediateRetry = true
    );

    void recordDisconnect(
        const std::string& nodeId,
        const std::string& endpoint,
        std::int64_t       now
    );

    void recordAttempt(
        const std::string& nodeId,
        std::int64_t       now
    );

    void recordSuccess(const std::string& nodeId);

    void recordFailure(
        const std::string& nodeId,
        std::int64_t       now
    );

    void quarantine(
        const std::string& nodeId,
        const std::string& reason,
        std::int64_t       now
    );

    void lift(const std::string& nodeId, std::int64_t now = 0);

    std::vector<PeerReconnectionState> candidatesForReconnect(
        std::int64_t now
    ) const;

    bool isTracked(const std::string& nodeId) const;
    bool isQuarantined(const std::string& nodeId) const;
    const PeerReconnectionState* state(const std::string& nodeId) const;

    std::size_t trackedCount()    const;
    std::size_t quarantineCount() const;

    std::string serialize() const;

    static std::int64_t backoffDelayForAttempt(std::uint32_t attempt);
    static bool isSafeNodeId(const std::string& nodeId);
    static bool isSafeEndpoint(const std::string& endpoint);

private:
    std::map<std::string, PeerReconnectionState> m_states;
};

/*
 * PeerExchangePayload is the data carried in a peer-exchange message.
 * A node sends its known active peers to help new nodes discover the network.
 */
struct PeerExchangeEntry {
    std::string nodeId;
    std::string endpoint;
    std::string fingerprint;
};

class PeerExchangeService {
public:
    /*
     * Build a peer-exchange payload from the current active peer list.
     * Caps output at maxPeers to limit message size.
     */
    static std::vector<PeerExchangeEntry> buildPayload(
        const std::vector<PeerMetadata>& activePeers,
        std::size_t                      maxPeers = 10
    );

    /*
     * Merge received peer-exchange entries into the local reconnection policy.
     * Ignores entries already tracked or quarantined.
     */
    static void mergeInto(
        const std::vector<PeerExchangeEntry>& entries,
        PeerReconnectionPolicy&               policy,
        std::int64_t                          now
    );

    static std::string serializePayload(
        const std::vector<PeerExchangeEntry>& entries
    );

    static std::vector<PeerExchangeEntry> deserializePayload(
        const std::string& serialized
    );
};

} // namespace nodo::p2p

#endif
