#ifndef NODO_P2P_TCP_TRANSPORT_HPP
#define NODO_P2P_TCP_TRANSPORT_HPP

#include "p2p/Peer.hpp"
#include "p2p/Transport.hpp"

#include <asio.hpp>
#include <cstdint>
#include <map>
#include <mutex>
#include <queue>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace nodo::p2p {

/*
 * TcpTransport implements an asynchronous non-blocking TCP transport using Asio.
 * It runs the Asio event loop in a background thread and maintains a thread-safe
 * queue of incoming messages for the main state machine's poll() loop.
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

private:
    struct PeerSession : public std::enable_shared_from_this<PeerSession> {
        asio::ip::tcp::socket socket;
        std::string peerNodeId;
        bool identified = false;

        // Buffers for reading frames
        std::uint32_t pendingHeader = 0;
        std::vector<unsigned char> pendingFrame;

        // Outbound write queue
        std::queue<std::vector<unsigned char>> writeQueue;
        bool writing = false;

        TcpTransport& transport;

        PeerSession(asio::ip::tcp::socket s, TcpTransport& t)
            : socket(std::move(s)), transport(t) {}
    };

    mutable std::mutex m_mutex;

    // Asio members
    asio::io_context m_ioContext;
    asio::executor_work_guard<asio::io_context::executor_type> m_workGuard;
    std::unique_ptr<asio::ip::tcp::acceptor> m_acceptor;
    std::thread m_ioThread;

    std::string m_localNodeId;
    PeerEndpoint m_localEndpoint;

    // Registered peer endpoints
    std::map<std::string, PeerEndpoint> m_peerEndpoints;

    // Active sessions
    std::map<std::string, std::shared_ptr<PeerSession>> m_connectionsByPeer;
    std::vector<std::shared_ptr<PeerSession>> m_unidentifiedSessions;

    // Incoming messages
    std::queue<TransportMessage> m_incomingQueue;

    void startAccept();
    void startRead(std::shared_ptr<PeerSession> session);
    void writeSession(std::shared_ptr<PeerSession> session, std::vector<unsigned char> data);
    void writeSessionLoop(std::shared_ptr<PeerSession> session);
    void handleSessionError(std::shared_ptr<PeerSession> session);
    void identifySession(std::shared_ptr<PeerSession> session, const std::string& nodeId);

    static bool isSafeNodeId(const std::string& nodeId);
    static bool isSafeHost(const std::string& host);
};

} // namespace nodo::p2p

#endif
