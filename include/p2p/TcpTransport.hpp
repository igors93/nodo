#ifndef NODO_P2P_TCP_TRANSPORT_HPP
#define NODO_P2P_TCP_TRANSPORT_HPP

#include "p2p/Peer.hpp"
#include "p2p/Transport.hpp"

#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace nodo::p2p {

/*
 * TcpTransport is the first real socket-backed transport for Nodo testnet
 * runtime experiments. It is synchronous and deterministic by design: tests and
 * local operators explicitly call poll() instead of relying on background
 * threads. Future production transports can use the same Transport interface
 * with event loops, encryption and peer discovery.
 */
class TcpTransport final : public Transport {
public:
    TcpTransport();
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

    void closeAll();

#ifdef _WIN32
    using SocketHandle = std::uintptr_t;
#else
    using SocketHandle = int;
#endif

private:
    SocketHandle m_listenFd;
    bool m_socketRuntimeReady;
    std::string m_localNodeId;
    PeerEndpoint m_localEndpoint;
    std::map<std::string, PeerEndpoint> m_peerEndpoints;
    std::map<std::string, SocketHandle> m_connectionsByPeer;
    std::vector<SocketHandle> m_unidentifiedInboundFds;
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
        SocketHandle fd
    );

    void closeFd(SocketHandle fd);
    void closePeerConnection(const std::string& remoteNodeId);

    static bool isSafeNodeId(const std::string& nodeId);
    static bool isSafeHost(const std::string& host);
};

} // namespace nodo::p2p

#endif
