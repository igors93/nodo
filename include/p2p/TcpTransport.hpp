#ifndef NODO_P2P_TCP_TRANSPORT_HPP
#define NODO_P2P_TCP_TRANSPORT_HPP

#include "p2p/Peer.hpp"
#include "p2p/AuthenticatedConnectionTransport.hpp"
#include "p2p/Transport.hpp"

#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace nodo::p2p {

class TcpIpRateLimitPolicy {
public:
    static constexpr std::size_t DEFAULT_BUCKET_CAPACITY = 16;
    static constexpr std::size_t DEFAULT_REFILL_TOKENS = 4;
    static constexpr std::int64_t DEFAULT_REFILL_INTERVAL_MILLISECONDS = 1'000;
    static constexpr std::int64_t DEFAULT_INITIAL_BACKOFF_MILLISECONDS = 1'000;
    static constexpr std::int64_t DEFAULT_MAX_BACKOFF_MILLISECONDS = 60'000;

    TcpIpRateLimitPolicy();
    TcpIpRateLimitPolicy(
        std::size_t bucketCapacity,
        std::size_t refillTokens,
        std::chrono::milliseconds refillInterval,
        std::chrono::milliseconds initialBackoff,
        std::chrono::milliseconds maxBackoff
    );

    std::size_t bucketCapacity() const;
    std::size_t refillTokens() const;
    std::chrono::milliseconds refillInterval() const;
    std::chrono::milliseconds initialBackoff() const;
    std::chrono::milliseconds maxBackoff() const;
    bool isValid() const;

private:
    std::size_t m_bucketCapacity;
    std::size_t m_refillTokens;
    std::chrono::milliseconds m_refillInterval;
    std::chrono::milliseconds m_initialBackoff;
    std::chrono::milliseconds m_maxBackoff;
};

class TcpCandidatePolicy {
public:
    static constexpr std::size_t DEFAULT_MAX_TOTAL = 64;
    static constexpr std::size_t DEFAULT_MAX_PER_IP = 8;
    static constexpr std::int64_t DEFAULT_TIMEOUT_MILLISECONDS = 10'000;

    TcpCandidatePolicy();
    TcpCandidatePolicy(
        std::size_t maxTotal,
        std::size_t maxPerIp,
        std::chrono::milliseconds authenticationTimeout
    );
    TcpCandidatePolicy(
        std::size_t maxTotal,
        std::size_t maxPerIp,
        std::chrono::milliseconds authenticationTimeout,
        TcpIpRateLimitPolicy ipRateLimit
    );

    std::size_t maxTotal() const;
    std::size_t maxPerIp() const;
    std::chrono::milliseconds authenticationTimeout() const;
    const TcpIpRateLimitPolicy& ipRateLimit() const;
    bool isValid() const;

private:
    std::size_t m_maxTotal;
    std::size_t m_maxPerIp;
    std::chrono::milliseconds m_authenticationTimeout;
    TcpIpRateLimitPolicy m_ipRateLimit;
};

/*
 * TcpTransport is the first real socket-backed transport for Nodo testnet
 * runtime experiments. It is synchronous and deterministic by design: tests and
 * local operators explicitly call poll() instead of relying on background
 * threads. Future production transports can use the same Transport interface
 * with event loops, encryption and peer discovery.
 */
class TcpTransport final
    : public Transport,
      public AuthenticatedConnectionTransport {
public:
    TcpTransport();
    explicit TcpTransport(TcpCandidatePolicy candidatePolicy);
    ~TcpTransport() override;

    TcpTransport(const TcpTransport&) = delete;
    TcpTransport& operator=(const TcpTransport&) = delete;

    TransportResult bind(
        const std::string& localNodeId,
        const PeerEndpoint& endpoint
    );

    TransportResult bind(
        const std::string& localNodeId,
        const std::string& host,
        std::uint16_t port
    );

    bool listening() const;
    const std::string& localNodeId() const;
    const PeerEndpoint& localEndpoint() const;
    std::uint16_t localPort() const;

    void registerPeerEndpoint(
        const std::string& remoteNodeId,
        const PeerEndpoint& endpoint
    );

    bool hasPeerEndpoint(
        const std::string& remoteNodeId
    ) const;

    std::vector<std::string> connectedPeers() const;

    std::size_t pendingCandidateCount() const;
    std::size_t pendingCandidateCountForIp(
        const std::string& remoteIp
    ) const;
    std::size_t expiredCandidateCount() const;
    std::size_t rateLimitedCandidateCount() const;
    std::size_t temporalRateLimitedConnectionCount() const;
    std::size_t ipAdmissionStateCount() const;
    bool ipBackedOff(const std::string& remoteIp) const;
    std::chrono::milliseconds ipBackoffRemaining(
        const std::string& remoteIp
    ) const;

    TransportResult connect(
        const std::string& localNodeId,
        const std::string& remoteNodeId
    ) override;

    TransportResult disconnect(
        const std::string& localNodeId,
        const std::string& remoteNodeId
    ) override;

    bool connected(
        const std::string& localNodeId,
        const std::string& remoteNodeId
    ) const override;

    TransportResult send(
        const TransportMessage& message
    ) override;

    std::optional<TransportMessage> poll(
        const std::string& localNodeId
    ) override;

    bool authenticateConnection(
        TransportConnectionId connectionId,
        const std::string& remoteNodeId
    ) override;

    bool rejectConnection(
        TransportConnectionId connectionId
    ) override;

    bool isConnectionAuthenticated(
        TransportConnectionId connectionId,
        const std::string& remoteNodeId
    ) const override;

    void closeAll();

#ifdef _WIN32
    using SocketHandle = std::uintptr_t;
#else
    using SocketHandle = int;
#endif

private:
    struct ManagedConnection {
        SocketHandle fd;
        TransportConnectionId id;
        bool authenticated;
    };

    struct CandidateConnection {
        SocketHandle fd;
        TransportConnectionId id;
        std::string claimedNodeId;
        std::string remoteIp;
        std::chrono::steady_clock::time_point acceptedAt;
    };

    struct IpAdmissionState {
        std::size_t tokens;
        std::size_t consecutiveLimitHits;
        std::chrono::steady_clock::time_point lastRefillAt;
        std::chrono::steady_clock::time_point blockedUntil;
        std::chrono::steady_clock::time_point lastActivityAt;
    };

    SocketHandle m_listenFd;
    bool m_socketRuntimeReady;
    std::string m_localNodeId;
    PeerEndpoint m_localEndpoint;
    std::map<std::string, PeerEndpoint> m_peerEndpoints;
    std::map<std::string, ManagedConnection> m_connectionsByPeer;
    std::map<TransportConnectionId, CandidateConnection>
        m_candidateInboundConnections;
    std::map<std::string, TransportConnectionId> m_candidateByPeer;
    std::map<std::string, std::size_t> m_candidateCountByIp;
    std::map<std::string, IpAdmissionState> m_ipAdmissionByAddress;
    TcpCandidatePolicy m_candidatePolicy;
    std::size_t m_expiredCandidateCount;
    std::size_t m_rateLimitedCandidateCount;
    std::size_t m_temporalRateLimitedConnectionCount;
    TransportConnectionId m_nextConnectionId;
    // Guards all public methods that access shared maps/fds.
    // Recursive because send() may call connect() internally.
    mutable std::recursive_mutex m_mutex;

    struct PollFdResult {
        enum class Status {
            NONE,
            MESSAGE,
            CLOSED
        };

        Status status{Status::NONE};
        std::optional<TransportMessage> message{};

        static PollFdResult none();
        static PollFdResult closed();
        static PollFdResult received(TransportMessage message);
    };

    TransportResult acceptAvailableConnections();
    PollFdResult pollFd(
        SocketHandle fd,
        bool unidentified
    );

    void rememberConnection(
        const std::string& remoteNodeId,
        SocketHandle fd,
        bool authenticated
    );

    TransportConnectionId nextConnectionId();
    SocketHandle socketForSend(
        const TransportMessage& message
    ) const;
    void closeCandidateConnection(TransportConnectionId connectionId);
    void pruneExpiredCandidateConnections(
        std::chrono::steady_clock::time_point now
    );
    void decrementCandidateIpCount(const std::string& remoteIp);
    bool consumeIpAdmissionToken(
        const std::string& remoteIp,
        std::chrono::steady_clock::time_point now
    );
    void recordSuccessfulIpAuthentication(const std::string& remoteIp);
    void pruneIdleIpAdmissionStates(
        std::chrono::steady_clock::time_point now
    );
    std::chrono::milliseconds backoffForLimitHits(
        std::size_t limitHits
    ) const;

    void closeFd(SocketHandle fd);
    void closePeerConnection(const std::string& remoteNodeId);

    static bool isSafeNodeId(const std::string& nodeId);
    static bool isSafeHost(const std::string& host);
};

} // namespace nodo::p2p

#endif
