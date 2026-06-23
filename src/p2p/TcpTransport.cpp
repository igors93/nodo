#include "p2p/TcpTransport.hpp"
#include "p2p/TcpTransportFrameCodec.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::p2p {

TcpTransport::TcpTransport()
    : m_workGuard(asio::make_work_guard(m_ioContext)) {}

TcpTransport::~TcpTransport() {
    m_workGuard.reset();
    m_ioContext.stop();
    if (m_ioThread.joinable()) {
        m_ioThread.join();
    }
    closeAll();
}

TransportResult TcpTransport::bind(
    const std::string& localNodeId,
    const PeerEndpoint& endpoint
) {
    if (!isSafeNodeId(localNodeId)) {
        return TransportResult(TransportStatus::REJECTED, "Local node id is invalid.");
    }

    if (!isSafeHost(endpoint.host())) {
        return TransportResult(TransportStatus::REJECTED, "Local TCP endpoint is invalid.");
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_acceptor) {
        return TransportResult(TransportStatus::REJECTED, "TCP transport is already bound.");
    }

    try {
        m_acceptor = std::make_unique<asio::ip::tcp::acceptor>(
            m_ioContext,
            asio::ip::tcp::endpoint(asio::ip::make_address(endpoint.host()), endpoint.port())
        );

        m_localNodeId = localNodeId;
        m_localEndpoint = PeerEndpoint(
            endpoint.host(),
            m_acceptor->local_endpoint().port()
        );

        startAccept();

        // Start I/O background thread
        m_ioThread = std::thread([this]() {
            m_ioContext.run();
        });

        return TransportResult(TransportStatus::SENT, "TCP transport bound and listening.");
    } catch (const std::exception& e) {
        m_acceptor.reset();
        return TransportResult(TransportStatus::REJECTED, std::string("Unable to bind TCP socket: ") + e.what());
    }
}

TransportResult TcpTransport::bind(
    const std::string& localNodeId,
    const std::string& host,
    std::uint16_t port
) {
    return bind(localNodeId, PeerEndpoint(host, port));
}

bool TcpTransport::listening() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_acceptor && m_acceptor->is_open();
}

const std::string& TcpTransport::localNodeId() const {
    return m_localNodeId;
}

const PeerEndpoint& TcpTransport::localEndpoint() const {
    return m_localEndpoint;
}

std::uint16_t TcpTransport::localPort() const {
    return m_localEndpoint.port();
}

void TcpTransport::registerPeerEndpoint(
    const std::string& remoteNodeId,
    const PeerEndpoint& endpoint
) {
    if (isSafeNodeId(remoteNodeId) && endpoint.isValid()) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_peerEndpoints[remoteNodeId] = endpoint;
    }
}

bool TcpTransport::hasPeerEndpoint(
    const std::string& remoteNodeId
) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_peerEndpoints.find(remoteNodeId) != m_peerEndpoints.end();
}

std::vector<std::string> TcpTransport::connectedPeers() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> peers;
    peers.reserve(m_connectionsByPeer.size());

    for (const auto& [peer, ignored] : m_connectionsByPeer) {
        (void)ignored;
        peers.push_back(peer);
    }

    return peers;
}

TransportResult TcpTransport::connect(
    const std::string& localNodeId,
    const std::string& remoteNodeId
) {
    if (localNodeId != m_localNodeId || !listening()) {
        return TransportResult(TransportStatus::REJECTED, "TCP transport must be bound before connecting.");
    }

    if (!isSafeNodeId(remoteNodeId) || remoteNodeId == localNodeId) {
        return TransportResult(TransportStatus::REJECTED, "Remote node id is invalid.");
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_connectionsByPeer.find(remoteNodeId) != m_connectionsByPeer.end()) {
            return TransportResult(TransportStatus::SENT, "TCP peer is already connected.");
        }
    }

    PeerEndpoint remoteEndpoint;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto found = m_peerEndpoints.find(remoteNodeId);
        if (found == m_peerEndpoints.end()) {
            return TransportResult(TransportStatus::NOT_CONNECTED, "No TCP endpoint registered for remote peer.");
        }
        remoteEndpoint = found->second;
    }

    try {
        asio::ip::tcp::socket socket(m_ioContext);
        asio::ip::tcp::endpoint endpoint(asio::ip::make_address(remoteEndpoint.host()), remoteEndpoint.port());
        
        asio::error_code ec;
        socket.connect(endpoint, ec);
        if (ec) {
            return TransportResult(TransportStatus::NOT_CONNECTED, "Unable to connect TCP peer: " + ec.message());
        }

        auto session = std::make_shared<PeerSession>(std::move(socket), *this);
        session->peerNodeId = remoteNodeId;
        session->identified = true;

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_connectionsByPeer[remoteNodeId] = session;
        }

        startRead(session);

        return TransportResult(TransportStatus::SENT, "TCP peer connected.");
    } catch (const std::exception& e) {
        return TransportResult(TransportStatus::REJECTED, std::string("Connection exception: ") + e.what());
    }
}

TransportResult TcpTransport::disconnect(
    const std::string& localNodeId,
    const std::string& remoteNodeId
) {
    if (localNodeId != m_localNodeId) {
        return TransportResult(TransportStatus::REJECTED, "Local node id does not match bound TCP transport.");
    }

    std::shared_ptr<PeerSession> sessionToClose;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto found = m_connectionsByPeer.find(remoteNodeId);
        if (found != m_connectionsByPeer.end()) {
            sessionToClose = found->second;
            m_connectionsByPeer.erase(found);
        }
    }

    if (sessionToClose) {
        asio::post(m_ioContext, [sessionToClose]() {
            asio::error_code ec;
            sessionToClose->socket.close(ec);
        });
    }

    return TransportResult(TransportStatus::SENT, "TCP peer disconnected.");
}

bool TcpTransport::connected(
    const std::string& localNodeId,
    const std::string& remoteNodeId
) const {
    if (localNodeId != m_localNodeId) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    return m_connectionsByPeer.find(remoteNodeId) != m_connectionsByPeer.end();
}

TransportResult TcpTransport::send(const TransportMessage& message) {
    if (!message.isValid()) {
        return TransportResult(TransportStatus::INVALID_MESSAGE, "TCP transport message is invalid.");
    }

    if (message.fromNodeId() != m_localNodeId) {
        return TransportResult(TransportStatus::REJECTED, "TCP transport refuses to send from a different local node id.");
    }

    std::shared_ptr<PeerSession> session;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto found = m_connectionsByPeer.find(message.toNodeId());
        if (found != m_connectionsByPeer.end()) {
            session = found->second;
        }
    }

    if (!session) {
        auto connResult = connect(m_localNodeId, message.toNodeId());
        if (!connResult.success()) {
            return connResult;
        }
        std::lock_guard<std::mutex> lock(m_mutex);
        auto found = m_connectionsByPeer.find(message.toNodeId());
        if (found != m_connectionsByPeer.end()) {
            session = found->second;
        }
    }

    if (!session) {
        return TransportResult(TransportStatus::NOT_CONNECTED, "TCP peer is not connected.");
    }

    const std::vector<unsigned char> frame =
        TcpTransportFrameCodec::encodeTransportMessage(message);

    if (frame.empty() || frame.size() > TcpTransportFrameCodec::MAX_TCP_FRAME_BYTES) {
        return TransportResult(TransportStatus::INVALID_MESSAGE, "TCP frame size is invalid.");
    }

    std::uint32_t frameSize = static_cast<std::uint32_t>(frame.size());
    std::vector<unsigned char> fullPacket(4 + frame.size());
    fullPacket[0] = static_cast<unsigned char>((frameSize >> 24) & 0xFF);
    fullPacket[1] = static_cast<unsigned char>((frameSize >> 16) & 0xFF);
    fullPacket[2] = static_cast<unsigned char>((frameSize >> 8) & 0xFF);
    fullPacket[3] = static_cast<unsigned char>(frameSize & 0xFF);
    std::copy(frame.begin(), frame.end(), fullPacket.begin() + 4);

    writeSession(session, std::move(fullPacket));

    return TransportResult(TransportStatus::SENT, "TCP frame queued/sent.");
}

std::optional<TransportMessage> TcpTransport::poll(const std::string& localNodeId) {
    if (localNodeId != m_localNodeId || !listening()) {
        return std::nullopt;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_incomingQueue.empty()) {
        return std::nullopt;
    }

    auto msg = std::move(m_incomingQueue.front());
    m_incomingQueue.pop();
    return msg;
}

void TcpTransport::closeAll() {
    std::vector<std::shared_ptr<PeerSession>> sessionsToClose;
    std::unique_ptr<asio::ip::tcp::acceptor> acceptorToClose;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        acceptorToClose = std::move(m_acceptor);

        for (auto& [peer, session] : m_connectionsByPeer) {
            sessionsToClose.push_back(session);
        }
        m_connectionsByPeer.clear();

        for (auto& session : m_unidentifiedSessions) {
            sessionsToClose.push_back(session);
        }
        m_unidentifiedSessions.clear();

        std::queue<TransportMessage> empty;
        std::swap(m_incomingQueue, empty);
    }

    if (!m_ioContext.stopped()) {
        asio::post(m_ioContext, [acceptorToClose = std::move(acceptorToClose), sessionsToClose = std::move(sessionsToClose)]() {
            asio::error_code ec;
            if (acceptorToClose) acceptorToClose->close(ec);
            for (auto& session : sessionsToClose) {
                session->socket.close(ec);
            }
        });
    } else {
        asio::error_code ec;
        if (acceptorToClose) acceptorToClose->close(ec);
        for (auto& session : sessionsToClose) {
            session->socket.close(ec);
        }
    }
}

void TcpTransport::startAccept() {
    m_acceptor->async_accept(
        [this](asio::error_code ec, asio::ip::tcp::socket socket) {
            if (!ec) {
                auto session = std::make_shared<PeerSession>(std::move(socket), *this);
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_unidentifiedSessions.push_back(session);
                }
                startRead(session);
            }
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_acceptor && m_acceptor->is_open()) {
                startAccept();
            }
        });
}

void TcpTransport::startRead(std::shared_ptr<PeerSession> session) {
    auto self = session->shared_from_this();
    asio::async_read(session->socket, asio::buffer(&session->pendingHeader, 4),
        [this, self](asio::error_code ec, std::size_t length) {
            (void)length;

            if (ec) {
                handleSessionError(self);
                return;
            }

            unsigned char bytes[4];
            std::memcpy(bytes, &self->pendingHeader, 4);
            std::uint32_t frameSize = (static_cast<std::uint32_t>(bytes[0]) << 24) |
                                      (static_cast<std::uint32_t>(bytes[1]) << 16) |
                                      (static_cast<std::uint32_t>(bytes[2]) << 8) |
                                      static_cast<std::uint32_t>(bytes[3]);

            if (frameSize == 0 || frameSize > TcpTransportFrameCodec::MAX_TCP_FRAME_BYTES) {
                handleSessionError(self);
                return;
            }

            self->pendingFrame.resize(frameSize);
            asio::async_read(self->socket, asio::buffer(self->pendingFrame),
                [this, self](asio::error_code ec, std::size_t length) {
                    (void)length;

                    if (ec) {
                        handleSessionError(self);
                        return;
                    }

                    try {
                        TransportMessage message =
                            TcpTransportFrameCodec::decodeTransportMessage(self->pendingFrame);

                        if (message.toNodeId() == m_localNodeId) {
                            std::lock_guard<std::mutex> lock(m_mutex);
                            if (!self->identified) {
                                identifySession(self, message.fromNodeId());
                            }
                            m_incomingQueue.push(message);
                        }
                    } catch (...) {
                        handleSessionError(self);
                        return;
                    }

                    startRead(self);
                });
        });
}

void TcpTransport::writeSession(std::shared_ptr<PeerSession> session, std::vector<unsigned char> data) {
    asio::post(m_ioContext, [this, session, data = std::move(data)]() mutable {
        session->writeQueue.push(std::move(data));
        if (!session->writing) {
            writeSessionLoop(session);
        }
    });
}

void TcpTransport::writeSessionLoop(std::shared_ptr<PeerSession> session) {
    if (session->writeQueue.empty()) {
        session->writing = false;
        return;
    }

    session->writing = true;
    auto self = session->shared_from_this();
    auto& data = session->writeQueue.front();

    asio::async_write(session->socket, asio::buffer(data),
        [this, self](asio::error_code ec, std::size_t length) {
            (void)length;

            if (ec) {
                handleSessionError(self);
                return;
            }

            self->writeQueue.pop();
            writeSessionLoop(self);
        });
}

void TcpTransport::handleSessionError(std::shared_ptr<PeerSession> session) {
    asio::error_code ec;
    session->socket.close(ec);

    std::lock_guard<std::mutex> lock(m_mutex);
    if (session->identified) {
        auto found = m_connectionsByPeer.find(session->peerNodeId);
        if (found != m_connectionsByPeer.end() && found->second == session) {
            m_connectionsByPeer.erase(found);
        }
    } else {
        auto it = std::find(m_unidentifiedSessions.begin(), m_unidentifiedSessions.end(), session);
        if (it != m_unidentifiedSessions.end()) {
            m_unidentifiedSessions.erase(it);
        }
    }
}

void TcpTransport::identifySession(std::shared_ptr<PeerSession> session, const std::string& nodeId) {
    session->peerNodeId = nodeId;
    session->identified = true;

    auto it = std::find(m_unidentifiedSessions.begin(), m_unidentifiedSessions.end(), session);
    if (it != m_unidentifiedSessions.end()) {
        m_unidentifiedSessions.erase(it);
    }

    auto existing = m_connectionsByPeer.find(nodeId);
    if (existing != m_connectionsByPeer.end()) {
        asio::error_code ec;
        existing->second->socket.close(ec);
    }

    m_connectionsByPeer[nodeId] = session;
}

bool TcpTransport::isSafeNodeId(const std::string& nodeId) {
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

        if (!allowed) {
            return false;
        }
    }

    return true;
}

bool TcpTransport::isSafeHost(const std::string& host) {
    if (host.empty() || host.size() > 200) {
        return false;
    }

    for (const char character : host) {
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

} // namespace nodo::p2p
