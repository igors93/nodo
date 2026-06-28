#include "p2p/TcpTransport.hpp"

#include "p2p/TcpTransportFrameCodec.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace nodo::p2p {

namespace {

using SocketHandle = TcpTransport::SocketHandle;

#ifdef _WIN32
constexpr SocketHandle INVALID_FD = static_cast<SocketHandle>(INVALID_SOCKET);
constexpr int SEND_FLAGS = 0;
using NativeSocket = SOCKET;
using SocketLength = int;
using IoctlAvailableBytes = u_long;
#else
constexpr SocketHandle INVALID_FD = -1;
#ifdef MSG_NOSIGNAL
constexpr int SEND_FLAGS = MSG_NOSIGNAL;
#else
constexpr int SEND_FLAGS = 0;
#endif
using NativeSocket = int;
using SocketLength = socklen_t;
using IoctlAvailableBytes = int;
#endif

constexpr int LISTEN_BACKLOG = 16;
constexpr std::size_t MAX_UNIDENTIFIED_INBOUND_CONNECTIONS = 64;
constexpr int TCP_CONNECT_TIMEOUT_MS = 1000;
constexpr int TCP_IO_WAIT_TIMEOUT_MS = 250;
constexpr std::uint32_t MAX_WIRE_FRAME_BYTES =
    static_cast<std::uint32_t>(TcpTransportFrameCodec::MAX_TCP_FRAME_BYTES);

#ifdef _WIN32
bool startupSocketRuntime() {
    WSADATA data;
    return WSAStartup(MAKEWORD(2, 2), &data) == 0;
}

void cleanupSocketRuntime() {
    WSACleanup();
}

bool isInterruptedSocketError() {
    return WSAGetLastError() == WSAEINTR;
}

bool isWouldBlockSocketError() {
    return WSAGetLastError() == WSAEWOULDBLOCK;
}

bool isConnectInProgressSocketError() {
    const int error = WSAGetLastError();
    return error == WSAEWOULDBLOCK ||
           error == WSAEINPROGRESS ||
           error == WSAEINVAL;
}

std::string lastSocketErrorMessage(const std::string& prefix) {
    std::ostringstream output;
    output << prefix << ": Winsock error " << WSAGetLastError();
    return output.str();
}

bool setNonBlocking(SocketHandle fd) {
    u_long mode = 1;
    return ioctlsocket(static_cast<SOCKET>(fd), FIONBIO, &mode) == 0;
}

void closeSocket(SocketHandle& fd) {
    if (fd != INVALID_FD) {
        closesocket(static_cast<SOCKET>(fd));
        fd = INVALID_FD;
    }
}

int socketAvailableBytes(SocketHandle fd, IoctlAvailableBytes& availableBytes) {
    return ioctlsocket(static_cast<SOCKET>(fd), FIONREAD, &availableBytes);
}

int socketOptionError(SocketHandle fd) {
    int error = 0;
    int length = sizeof(error);
    if (getsockopt(
            static_cast<SOCKET>(fd),
            SOL_SOCKET,
            SO_ERROR,
            reinterpret_cast<char*>(&error),
            &length
        ) != 0) {
        return WSAGetLastError();
    }
    return error;
}
#else
bool startupSocketRuntime() {
    return true;
}

void cleanupSocketRuntime() {}

bool isInterruptedSocketError() {
    return errno == EINTR;
}

bool isWouldBlockSocketError() {
    return errno == EAGAIN || errno == EWOULDBLOCK;
}

bool isConnectInProgressSocketError() {
    return errno == EINPROGRESS;
}

std::string lastSocketErrorMessage(const std::string& prefix) {
    std::ostringstream output;
    output << prefix << ": " << std::strerror(errno);
    return output.str();
}

bool setNonBlocking(SocketHandle fd) {
    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }

    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

void closeSocket(SocketHandle& fd) {
    if (fd != INVALID_FD) {
        ::close(fd);
        fd = INVALID_FD;
    }
}

int socketAvailableBytes(SocketHandle fd, IoctlAvailableBytes& availableBytes) {
    return ::ioctl(fd, FIONREAD, &availableBytes);
}

int socketOptionError(SocketHandle fd) {
    int error = 0;
    socklen_t length = sizeof(error);
    if (::getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &length) != 0) {
        return errno;
    }
    return error;
}
#endif

bool waitForSocketReady(
    SocketHandle fd,
    bool writeReady,
    int timeoutMs
) {
    while (true) {
        fd_set readSet;
        fd_set writeSet;
        FD_ZERO(&readSet);
        FD_ZERO(&writeSet);

        if (writeReady) {
            FD_SET(static_cast<NativeSocket>(fd), &writeSet);
        } else {
            FD_SET(static_cast<NativeSocket>(fd), &readSet);
        }

        timeval timeout;
        timeout.tv_sec = timeoutMs / 1000;
        timeout.tv_usec = (timeoutMs % 1000) * 1000;

        const int result = select(
#ifdef _WIN32
            0,
#else
            fd + 1,
#endif
            writeReady ? nullptr : &readSet,
            writeReady ? &writeSet : nullptr,
            nullptr,
            &timeout
        );

        if (result < 0 && isInterruptedSocketError()) {
            continue;
        }

        if (result <= 0) {
            return false;
        }

        return writeReady
            ? FD_ISSET(static_cast<NativeSocket>(fd), &writeSet)
            : FD_ISSET(static_cast<NativeSocket>(fd), &readSet);
    }
}

std::uint32_t decodeU32BigEndian(const unsigned char* data) {
    return (static_cast<std::uint32_t>(data[0]) << 24) |
           (static_cast<std::uint32_t>(data[1]) << 16) |
           (static_cast<std::uint32_t>(data[2]) << 8) |
           static_cast<std::uint32_t>(data[3]);
}

std::array<unsigned char, 4> encodeU32BigEndian(std::uint32_t value) {
    return {
        static_cast<unsigned char>((value >> 24) & 0xFF),
        static_cast<unsigned char>((value >> 16) & 0xFF),
        static_cast<unsigned char>((value >> 8) & 0xFF),
        static_cast<unsigned char>(value & 0xFF)
    };
}

bool writeAll(SocketHandle fd, const unsigned char* data, std::size_t size) {
    std::size_t written = 0;

    while (written < size) {
        const int result = ::send(
            static_cast<NativeSocket>(fd),
            reinterpret_cast<const char*>(data + written),
            static_cast<int>(size - written),
            SEND_FLAGS
        );

        if (result < 0) {
            if (isInterruptedSocketError()) {
                continue;
            }
            if (isWouldBlockSocketError() &&
                waitForSocketReady(fd, true, TCP_IO_WAIT_TIMEOUT_MS)) {
                continue;
            }
            return false;
        }

        if (result == 0) {
            return false;
        }

        written += static_cast<std::size_t>(result);
    }

    return true;
}

bool readAll(SocketHandle fd, unsigned char* data, std::size_t size) {
    std::size_t read = 0;

    while (read < size) {
        const int result = ::recv(
            static_cast<NativeSocket>(fd),
            reinterpret_cast<char*>(data + read),
            static_cast<int>(size - read),
            0
        );

        if (result < 0) {
            if (isInterruptedSocketError()) {
                continue;
            }
            if (isWouldBlockSocketError() &&
                waitForSocketReady(fd, false, TCP_IO_WAIT_TIMEOUT_MS)) {
                continue;
            }
            return false;
        }

        if (result == 0) {
            return false;
        }

        read += static_cast<std::size_t>(result);
    }

    return true;
}

bool socketHasReadableData(SocketHandle fd) {
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(static_cast<NativeSocket>(fd), &readSet);

    timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    const int result = select(
#ifdef _WIN32
        0,
#else
        fd + 1,
#endif
        &readSet,
        nullptr,
        nullptr,
        &timeout
    );

    return result > 0 && FD_ISSET(static_cast<NativeSocket>(fd), &readSet);
}

} // namespace

TcpTransport::TcpTransport()
    : m_listenFd(INVALID_FD),
      m_socketRuntimeReady(startupSocketRuntime()),
      m_localNodeId(),
      m_localEndpoint(),
      m_peerEndpoints(),
      m_connectionsByPeer(),
      m_candidateInboundConnections(),
      m_candidateByPeer(),
      m_nextConnectionId(1) {}

TcpTransport::~TcpTransport() {
    closeAll();
    if (m_socketRuntimeReady) {
        cleanupSocketRuntime();
    }
}

TransportResult TcpTransport::bind(
    const std::string& localNodeId,
    const PeerEndpoint& endpoint
) {
    if (!isSafeNodeId(localNodeId)) {
        return TransportResult(
            TransportStatus::REJECTED,
            "Local node id is invalid."
        );
    }

    if (!isSafeHost(endpoint.host())) {
        return TransportResult(
            TransportStatus::REJECTED,
            "Local TCP endpoint is invalid."
        );
    }

    if (m_listenFd != INVALID_FD) {
        return TransportResult(
            TransportStatus::REJECTED,
            "TCP transport is already bound."
        );
    }

    if (!m_socketRuntimeReady) {
        return TransportResult(
            TransportStatus::REJECTED,
            "Unable to initialize TCP socket runtime."
        );
    }

    const SocketHandle fd =
        static_cast<SocketHandle>(::socket(AF_INET, SOCK_STREAM, 0));
    if (fd == INVALID_FD) {
        return TransportResult(
            TransportStatus::REJECTED,
            lastSocketErrorMessage("Unable to create TCP socket")
        );
    }

    int reuse = 1;
    (void)::setsockopt(
        static_cast<NativeSocket>(fd),
        SOL_SOCKET,
        SO_REUSEADDR,
        reinterpret_cast<const char*>(&reuse),
        static_cast<SocketLength>(sizeof(reuse))
    );

    sockaddr_in address;
    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(endpoint.port());

    if (::inet_pton(AF_INET, endpoint.host().c_str(), &address.sin_addr) != 1) {
        SocketHandle closeFd = fd;
        closeSocket(closeFd);
        return TransportResult(
            TransportStatus::REJECTED,
            "TCP endpoint host must be an IPv4 address for this runtime phase."
        );
    }

    if (::bind(
            static_cast<NativeSocket>(fd),
            reinterpret_cast<sockaddr*>(&address),
            static_cast<SocketLength>(sizeof(address))
        ) != 0) {
        SocketHandle closeFd = fd;
        closeSocket(closeFd);
        return TransportResult(
            TransportStatus::REJECTED,
            lastSocketErrorMessage("Unable to bind TCP socket")
        );
    }

    if (::listen(static_cast<NativeSocket>(fd), LISTEN_BACKLOG) != 0) {
        SocketHandle closeFd = fd;
        closeSocket(closeFd);
        return TransportResult(
            TransportStatus::REJECTED,
            lastSocketErrorMessage("Unable to listen on TCP socket")
        );
    }

    if (!setNonBlocking(fd)) {
        SocketHandle closeFd = fd;
        closeSocket(closeFd);
        return TransportResult(
            TransportStatus::REJECTED,
            lastSocketErrorMessage("Unable to set TCP socket nonblocking")
        );
    }

    sockaddr_in boundAddress;
    std::memset(&boundAddress, 0, sizeof(boundAddress));
    SocketLength boundLength = static_cast<SocketLength>(sizeof(boundAddress));
    if (::getsockname(
            static_cast<NativeSocket>(fd),
            reinterpret_cast<sockaddr*>(&boundAddress),
            &boundLength
        ) != 0) {
        SocketHandle closeFd = fd;
        closeSocket(closeFd);
        return TransportResult(
            TransportStatus::REJECTED,
            lastSocketErrorMessage("Unable to read bound TCP endpoint")
        );
    }

    m_listenFd = fd;
    m_localNodeId = localNodeId;
    m_localEndpoint = PeerEndpoint(
        endpoint.host(),
        ntohs(boundAddress.sin_port)
    );

    return TransportResult(
        TransportStatus::SENT,
        "TCP transport bound and listening."
    );
}

TransportResult TcpTransport::bind(
    const std::string& localNodeId,
    const std::string& host,
    std::uint16_t port
) {
    return bind(localNodeId, PeerEndpoint(host, port));
}

bool TcpTransport::listening() const {
    return m_listenFd != INVALID_FD;
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
        m_peerEndpoints[remoteNodeId] = endpoint;
    }
}

bool TcpTransport::hasPeerEndpoint(
    const std::string& remoteNodeId
) const {
    return m_peerEndpoints.find(remoteNodeId) != m_peerEndpoints.end();
}

std::vector<std::string> TcpTransport::connectedPeers() const {
    std::vector<std::string> peers;
    peers.reserve(m_connectionsByPeer.size());

    for (const auto& [peer, connection] : m_connectionsByPeer) {
        (void)connection;
        peers.push_back(peer);
    }

    return peers;
}

TransportResult TcpTransport::connect(
    const std::string& localNodeId,
    const std::string& remoteNodeId
) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (localNodeId != m_localNodeId || !listening()) {
        return TransportResult(
            TransportStatus::REJECTED,
            "TCP transport must be bound before connecting."
        );
    }

    if (!isSafeNodeId(remoteNodeId) || remoteNodeId == localNodeId) {
        return TransportResult(
            TransportStatus::REJECTED,
            "Remote node id is invalid."
        );
    }

    if (connected(localNodeId, remoteNodeId)) {
        return TransportResult(
            TransportStatus::SENT,
            "TCP peer is already connected."
        );
    }

    const auto endpoint = m_peerEndpoints.find(remoteNodeId);
    if (endpoint == m_peerEndpoints.end()) {
        return TransportResult(
            TransportStatus::NOT_CONNECTED,
            "No TCP endpoint registered for remote peer."
        );
    }

    if (!m_socketRuntimeReady) {
        return TransportResult(
            TransportStatus::REJECTED,
            "Unable to initialize TCP socket runtime."
        );
    }

    const SocketHandle fd =
        static_cast<SocketHandle>(::socket(AF_INET, SOCK_STREAM, 0));
    if (fd == INVALID_FD) {
        return TransportResult(
            TransportStatus::REJECTED,
            lastSocketErrorMessage("Unable to create outbound TCP socket")
        );
    }

    if (!setNonBlocking(fd)) {
        SocketHandle closeFd = fd;
        closeSocket(closeFd);
        return TransportResult(
            TransportStatus::REJECTED,
            lastSocketErrorMessage("Unable to set outbound TCP socket nonblocking")
        );
    }

    sockaddr_in address;
    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(endpoint->second.port());

    if (::inet_pton(AF_INET, endpoint->second.host().c_str(), &address.sin_addr) != 1) {
        SocketHandle closeFd = fd;
        closeSocket(closeFd);
        return TransportResult(
            TransportStatus::REJECTED,
            "Remote TCP endpoint host must be an IPv4 address."
        );
    }

    if (::connect(
            static_cast<NativeSocket>(fd),
            reinterpret_cast<sockaddr*>(&address),
            static_cast<SocketLength>(sizeof(address))
        ) != 0) {
        if (!isConnectInProgressSocketError()) {
            SocketHandle closeFd = fd;
            closeSocket(closeFd);
            return TransportResult(
                TransportStatus::NOT_CONNECTED,
                lastSocketErrorMessage("Unable to connect TCP peer")
            );
        }

        if (!waitForSocketReady(fd, true, TCP_CONNECT_TIMEOUT_MS)) {
            SocketHandle closeFd = fd;
            closeSocket(closeFd);
            return TransportResult(
                TransportStatus::NOT_CONNECTED,
                "TCP connect timed out."
            );
        }

        const int socketError = socketOptionError(fd);
        if (socketError != 0) {
            SocketHandle closeFd = fd;
            closeSocket(closeFd);
            std::ostringstream reason;
            reason << "Unable to connect TCP peer: socket error "
                   << socketError;
            return TransportResult(
                TransportStatus::NOT_CONNECTED,
                reason.str()
            );
        }
    }

    rememberConnection(remoteNodeId, fd, false);

    return TransportResult(
        TransportStatus::SENT,
        "TCP peer connected."
    );
}

TransportResult TcpTransport::disconnect(
    const std::string& localNodeId,
    const std::string& remoteNodeId
) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (localNodeId != m_localNodeId) {
        return TransportResult(
            TransportStatus::REJECTED,
            "Local node id does not match bound TCP transport."
        );
    }

    closePeerConnection(remoteNodeId);

    return TransportResult(
        TransportStatus::SENT,
        "TCP peer disconnected."
    );
}

bool TcpTransport::connected(
    const std::string& localNodeId,
    const std::string& remoteNodeId
) const {
    if (localNodeId != m_localNodeId) {
        return false;
    }
    return m_connectionsByPeer.find(remoteNodeId) != m_connectionsByPeer.end();
}

TransportResult TcpTransport::send(
    const TransportMessage& message
) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (!message.isValid()) {
        return TransportResult(
            TransportStatus::INVALID_MESSAGE,
            "TCP transport message is invalid."
        );
    }

    if (message.fromNodeId() != m_localNodeId) {
        return TransportResult(
            TransportStatus::REJECTED,
            "TCP transport refuses to send from a different local node id."
        );
    }

    SocketHandle destinationFd = socketForSend(message);
    if (destinationFd == INVALID_FD &&
        !connected(m_localNodeId, message.toNodeId())) {
        const TransportResult connectionResult =
            connect(m_localNodeId, message.toNodeId());

        if (!connectionResult.success()) {
            return connectionResult;
        }
    }

    destinationFd = socketForSend(message);
    if (destinationFd == INVALID_FD) {
        return TransportResult(
            TransportStatus::NOT_CONNECTED,
            "TCP peer is not connected."
        );
    }

    const std::vector<unsigned char> frame =
        TcpTransportFrameCodec::encodeTransportMessage(message);

    if (frame.empty() || frame.size() > MAX_WIRE_FRAME_BYTES) {
        return TransportResult(
            TransportStatus::INVALID_MESSAGE,
            "TCP frame size is invalid."
        );
    }

    const auto lengthPrefix =
        encodeU32BigEndian(static_cast<std::uint32_t>(frame.size()));

    // Send prefix and body as one write so Nagle's algorithm cannot hold the
    // body waiting for an ACK on the prefix, which would stall the receiver.
    std::vector<unsigned char> wireData;
    wireData.reserve(lengthPrefix.size() + frame.size());
    wireData.insert(wireData.end(), lengthPrefix.begin(), lengthPrefix.end());
    wireData.insert(wireData.end(), frame.begin(), frame.end());

    if (!writeAll(destinationFd, wireData.data(), wireData.size())) {
        closeFd(destinationFd);
        return TransportResult(
            TransportStatus::NOT_CONNECTED,
            "Unable to write full TCP frame."
        );
    }

    return TransportResult(
        TransportStatus::SENT,
        "TCP frame sent."
    );
}

std::optional<TransportMessage> TcpTransport::poll(
    const std::string& localNodeId
) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (localNodeId != m_localNodeId || !listening()) {
        return std::nullopt;
    }

    (void)acceptAvailableConnections();

    for (auto iterator = m_connectionsByPeer.begin();
         iterator != m_connectionsByPeer.end();) {
        const SocketHandle fd = iterator->second.fd;
        PollFdResult result = pollFd(fd, false);

        if (result.status == PollFdResult::Status::MESSAGE) {
            // An active socket may never change the identity it authenticated.
            if (!result.message.has_value() ||
                result.message->fromNodeId() != iterator->first) {
                SocketHandle closedFd = fd;
                closeSocket(closedFd);
                iterator = m_connectionsByPeer.erase(iterator);
                continue;
            }

            const TransportMessage& message = *result.message;
            return TransportMessage(
                message.fromNodeId(),
                message.toNodeId(),
                message.envelope(),
                message.sentAt(),
                iterator->second.id
            );
        }

        if (result.status == PollFdResult::Status::CLOSED) {
            SocketHandle closedFd = fd;
            closeSocket(closedFd);
            iterator = m_connectionsByPeer.erase(iterator);
            continue;
        }

        ++iterator;
    }

    for (auto iterator = m_candidateInboundConnections.begin();
         iterator != m_candidateInboundConnections.end();) {
        const SocketHandle fd = iterator->second.fd;
        PollFdResult result = pollFd(fd, true);

        if (result.status == PollFdResult::Status::MESSAGE) {
            const std::string fromId = result.message->fromNodeId();
            if (!isSafeNodeId(fromId) ||
                (!iterator->second.claimedNodeId.empty() &&
                 iterator->second.claimedNodeId != fromId)) {
                const TransportConnectionId rejectedId = iterator->first;
                ++iterator;
                closeCandidateConnection(rejectedId);
                continue;
            }

            if (iterator->second.claimedNodeId.empty()) {
                const auto claimed = m_candidateByPeer.find(fromId);
                if (claimed != m_candidateByPeer.end() &&
                    claimed->second != iterator->first) {
                    const TransportConnectionId rejectedId = iterator->first;
                    ++iterator;
                    closeCandidateConnection(rejectedId);
                    continue;
                }
                iterator->second.claimedNodeId = fromId;
                m_candidateByPeer[fromId] = iterator->first;
            }

            const TransportConnectionId connectionId = iterator->first;
            const TransportMessage& message = *result.message;
            return TransportMessage(
                message.fromNodeId(),
                message.toNodeId(),
                message.envelope(),
                message.sentAt(),
                connectionId
            );
        }

        if (result.status == PollFdResult::Status::CLOSED) {
            const TransportConnectionId closedId = iterator->first;
            ++iterator;
            closeCandidateConnection(closedId);
            continue;
        }

        ++iterator;
    }

    return std::nullopt;
}

bool TcpTransport::authenticateConnection(
    TransportConnectionId connectionId,
    const std::string& remoteNodeId
) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (connectionId == 0 || !isSafeNodeId(remoteNodeId)) return false;

    const auto active = m_connectionsByPeer.find(remoteNodeId);
    if (active != m_connectionsByPeer.end() &&
        active->second.id == connectionId) {
        active->second.authenticated = true;
        return true;
    }

    const auto candidate = m_candidateInboundConnections.find(connectionId);
    if (candidate == m_candidateInboundConnections.end() ||
        candidate->second.claimedNodeId != remoteNodeId) {
        return false;
    }

    const ManagedConnection promoted{
        candidate->second.fd,
        candidate->second.id,
        true
    };
    if (active != m_connectionsByPeer.end()) {
        SocketHandle oldFd = active->second.fd;
        closeSocket(oldFd);
        m_connectionsByPeer.erase(active);
    }
    m_candidateByPeer.erase(remoteNodeId);
    m_candidateInboundConnections.erase(candidate);
    m_connectionsByPeer[remoteNodeId] = promoted;
    return true;
}

bool TcpTransport::rejectConnection(TransportConnectionId connectionId) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (connectionId == 0) return false;

    const auto candidate = m_candidateInboundConnections.find(connectionId);
    if (candidate != m_candidateInboundConnections.end()) {
        closeCandidateConnection(connectionId);
        return true;
    }

    for (auto iterator = m_connectionsByPeer.begin();
         iterator != m_connectionsByPeer.end(); ++iterator) {
        if (iterator->second.id == connectionId) {
            SocketHandle fd = iterator->second.fd;
            closeSocket(fd);
            m_connectionsByPeer.erase(iterator);
            return true;
        }
    }
    return false;
}

bool TcpTransport::isConnectionAuthenticated(
    TransportConnectionId connectionId,
    const std::string& remoteNodeId
) const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    const auto found = m_connectionsByPeer.find(remoteNodeId);
    return found != m_connectionsByPeer.end() &&
           found->second.id == connectionId &&
           found->second.authenticated;
}

void TcpTransport::closeAll() {
    for (auto& [peer, connection] : m_connectionsByPeer) {
        (void)peer;
        closeSocket(connection.fd);
    }
    m_connectionsByPeer.clear();

    for (auto& [id, candidate] : m_candidateInboundConnections) {
        (void)id;
        closeSocket(candidate.fd);
    }
    m_candidateInboundConnections.clear();
    m_candidateByPeer.clear();

    closeSocket(m_listenFd);
}

/**
 * Accepts incoming TCP connections from the non-blocking listener socket.
 * Registers successfully connected peers into the unidentified connections pool
 * to await the initial handshake and protocol identification.
 */
TransportResult TcpTransport::acceptAvailableConnections() {
    if (m_listenFd == INVALID_FD) {
        return TransportResult(
            TransportStatus::REJECTED,
            "TCP listener is not bound."
        );
    }

    while (true) {
        sockaddr_in remoteAddress;
        std::memset(&remoteAddress, 0, sizeof(remoteAddress));
        SocketLength remoteLength = static_cast<SocketLength>(sizeof(remoteAddress));

        const SocketHandle fd = static_cast<SocketHandle>(::accept(
            static_cast<NativeSocket>(m_listenFd),
            reinterpret_cast<sockaddr*>(&remoteAddress),
            &remoteLength
        ));

        if (fd == INVALID_FD) {
            if (isWouldBlockSocketError()) {
                break;
            }

            return TransportResult(
                TransportStatus::REJECTED,
                lastSocketErrorMessage("Unable to accept TCP peer")
            );
        }

        if (!setNonBlocking(fd)) {
            SocketHandle closeFd = fd;
            closeSocket(closeFd);
            continue;
        }

        if (m_candidateInboundConnections.size() >=
            MAX_UNIDENTIFIED_INBOUND_CONNECTIONS) {
            SocketHandle closeFd = fd;
            closeSocket(closeFd);
            continue;
        }

        const TransportConnectionId connectionId = nextConnectionId();
        if (connectionId == 0) {
            SocketHandle closeFd = fd;
            closeSocket(closeFd);
            continue;
        }
        m_candidateInboundConnections.emplace(
            connectionId,
            CandidateConnection{fd, connectionId, ""}
        );
    }

    return TransportResult(
        TransportStatus::SENT,
        "TCP accept cycle completed."
    );
}

TcpTransport::PollFdResult TcpTransport::PollFdResult::none() {
    return PollFdResult{};
}

TcpTransport::PollFdResult TcpTransport::PollFdResult::closed() {
    PollFdResult result;
    result.status = Status::CLOSED;
    return result;
}

TcpTransport::PollFdResult TcpTransport::PollFdResult::received(
    TransportMessage message
) {
    PollFdResult result;
    result.status = Status::MESSAGE;
    result.message = std::move(message);
    return result;
}

TcpTransport::PollFdResult TcpTransport::pollFd(
    SocketHandle fd,
    bool unidentified
) {
    (void)unidentified;

    const bool hasData = socketHasReadableData(fd);
    if (fd == INVALID_FD || !hasData) {
        return PollFdResult::none();
    }

    IoctlAvailableBytes availableBytes = 0;
    if (socketAvailableBytes(fd, availableBytes) != 0) {
        if (isInterruptedSocketError() || isWouldBlockSocketError()) {
            return PollFdResult::none();
        }

        return PollFdResult::closed();
    }

    unsigned char header[4] = {0, 0, 0, 0};
    const int peeked = ::recv(
        static_cast<NativeSocket>(fd),
        reinterpret_cast<char*>(header),
        static_cast<int>(sizeof(header)),
        MSG_PEEK
    );
    if (peeked == 0) {
        return PollFdResult::closed();
    }

    if (peeked < 0) {
        if (isInterruptedSocketError() || isWouldBlockSocketError()) {
            return PollFdResult::none();
        }

        return PollFdResult::closed();
    }

    if (peeked < static_cast<int>(sizeof(header))) {
        return PollFdResult::none();
    }

    const std::uint32_t frameSize = decodeU32BigEndian(header);
    if (frameSize == 0 || frameSize > MAX_WIRE_FRAME_BYTES) {
        return PollFdResult::closed();
    }

    if (availableBytes < static_cast<IoctlAvailableBytes>(sizeof(header) + frameSize)) {
        return PollFdResult::none();
    }

    unsigned char consumedHeader[4] = {0, 0, 0, 0};
    if (!readAll(fd, consumedHeader, sizeof(consumedHeader))) {
        return PollFdResult::closed();
    }

    std::vector<unsigned char> frame(frameSize);
    if (!readAll(fd, frame.data(), frame.size())) {
        return PollFdResult::closed();
    }

    try {
        TransportMessage message =
            TcpTransportFrameCodec::decodeTransportMessage(frame);

        if (message.toNodeId() != m_localNodeId) {
            return PollFdResult::closed();
        }

        return PollFdResult::received(std::move(message));
    } catch (...) {
        return PollFdResult::closed();
    }
}

void TcpTransport::rememberConnection(
    const std::string& remoteNodeId,
    SocketHandle fd,
    bool authenticated
) {
    if (!isSafeNodeId(remoteNodeId) || fd == INVALID_FD) {
        return;
    }

    const auto existing = m_connectionsByPeer.find(remoteNodeId);
    if (existing != m_connectionsByPeer.end() && existing->second.fd != fd) {
        SocketHandle oldFd = existing->second.fd;
        closeSocket(oldFd);
    }

    const TransportConnectionId connectionId = nextConnectionId();
    if (connectionId == 0) {
        SocketHandle closeFd = fd;
        closeSocket(closeFd);
        return;
    }
    m_connectionsByPeer[remoteNodeId] =
        ManagedConnection{fd, connectionId, authenticated};
}

TransportConnectionId TcpTransport::nextConnectionId() {
    for (std::size_t attempt = 0; attempt < 1024; ++attempt) {
        const TransportConnectionId candidate = m_nextConnectionId++;
        if (m_nextConnectionId == 0) m_nextConnectionId = 1;
        if (candidate == 0) continue;

        bool used = m_candidateInboundConnections.find(candidate) !=
                    m_candidateInboundConnections.end();
        for (const auto& [peer, connection] : m_connectionsByPeer) {
            (void)peer;
            if (connection.id == candidate) {
                used = true;
                break;
            }
        }
        if (!used) return candidate;
    }
    return 0;
}

SocketHandle TcpTransport::socketForSend(
    const TransportMessage& message
) const {
    if (message.hasConnectionId()) {
        const auto active = m_connectionsByPeer.find(message.toNodeId());
        if (active != m_connectionsByPeer.end() &&
            active->second.id == message.connectionId()) {
            return active->second.fd;
        }
        const auto candidate =
            m_candidateInboundConnections.find(message.connectionId());
        if (candidate != m_candidateInboundConnections.end() &&
            candidate->second.claimedNodeId == message.toNodeId()) {
            return candidate->second.fd;
        }
        return INVALID_FD;
    }

    const NetworkMessageType type = message.envelope().messageType();
    const bool handshake = type == NetworkMessageType::PEER_CHALLENGE ||
                           type == NetworkMessageType::PEER_HELLO;
    if (handshake) {
        const auto candidateId = m_candidateByPeer.find(message.toNodeId());
        if (candidateId != m_candidateByPeer.end()) {
            const auto candidate =
                m_candidateInboundConnections.find(candidateId->second);
            if (candidate != m_candidateInboundConnections.end()) {
                return candidate->second.fd;
            }
        }
    }

    const auto active = m_connectionsByPeer.find(message.toNodeId());
    return active == m_connectionsByPeer.end()
        ? INVALID_FD
        : active->second.fd;
}

void TcpTransport::closeCandidateConnection(
    TransportConnectionId connectionId
) {
    const auto found = m_candidateInboundConnections.find(connectionId);
    if (found == m_candidateInboundConnections.end()) return;
    if (!found->second.claimedNodeId.empty()) {
        const auto claimed =
            m_candidateByPeer.find(found->second.claimedNodeId);
        if (claimed != m_candidateByPeer.end() &&
            claimed->second == connectionId) {
            m_candidateByPeer.erase(claimed);
        }
    }
    SocketHandle fd = found->second.fd;
    closeSocket(fd);
    m_candidateInboundConnections.erase(found);
}

void TcpTransport::closeFd(SocketHandle fd) {
    if (fd == INVALID_FD) {
        return;
    }

    for (auto iterator = m_connectionsByPeer.begin();
         iterator != m_connectionsByPeer.end();) {
        if (iterator->second.fd == fd) {
            SocketHandle socketFd = iterator->second.fd;
            closeSocket(socketFd);
            iterator = m_connectionsByPeer.erase(iterator);
        } else {
            ++iterator;
        }
    }

    for (auto iterator = m_candidateInboundConnections.begin();
         iterator != m_candidateInboundConnections.end(); ++iterator) {
        if (iterator->second.fd == fd) {
            const TransportConnectionId connectionId = iterator->first;
            closeCandidateConnection(connectionId);
            return;
        }
    }
}

void TcpTransport::closePeerConnection(
    const std::string& remoteNodeId
) {
    const auto found = m_connectionsByPeer.find(remoteNodeId);
    if (found != m_connectionsByPeer.end()) {
        SocketHandle fd = found->second.fd;
        closeSocket(fd);
        m_connectionsByPeer.erase(found);
    }

    const auto candidate = m_candidateByPeer.find(remoteNodeId);
    if (candidate != m_candidateByPeer.end()) {
        closeCandidateConnection(candidate->second);
    }
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
    return !host.empty() && host.size() <= 255;
}

} // namespace nodo::p2p
