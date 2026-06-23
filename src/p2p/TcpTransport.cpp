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
#endif

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
      m_unidentifiedInboundFds() {}

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
        SocketHandle closeFd = fd;
        closeSocket(closeFd);
        return TransportResult(
            TransportStatus::NOT_CONNECTED,
            lastSocketErrorMessage("Unable to connect TCP peer")
        );
    }

    (void)setNonBlocking(fd);
    rememberConnection(remoteNodeId, fd);

    return TransportResult(
        TransportStatus::SENT,
        "TCP peer connected."
    );
}

TransportResult TcpTransport::disconnect(
    const std::string& localNodeId,
    const std::string& remoteNodeId
) {
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
    (void)localNodeId;
    return m_connectionsByPeer.find(remoteNodeId) != m_connectionsByPeer.end();
}

TransportResult TcpTransport::send(
    const TransportMessage& message
) {
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

    if (!connected(m_localNodeId, message.toNodeId())) {
        const TransportResult connectionResult =
            connect(m_localNodeId, message.toNodeId());

        if (!connectionResult.success()) {
            return connectionResult;
        }
    }

    const auto found = m_connectionsByPeer.find(message.toNodeId());
    if (found == m_connectionsByPeer.end()) {
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

    if (!writeAll(found->second, lengthPrefix.data(), lengthPrefix.size()) ||
        !writeAll(found->second, frame.data(), frame.size())) {
        closePeerConnection(message.toNodeId());
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
    if (localNodeId != m_localNodeId || !listening()) {
        return std::nullopt;
    }

    (void)acceptAvailableConnections();

    for (auto iterator = m_connectionsByPeer.begin();
         iterator != m_connectionsByPeer.end();) {
        const SocketHandle fd = iterator->second;
        auto message = pollFd(fd, false);

        if (message.has_value()) {
            return message;
        }

        ++iterator;
    }

    for (auto iterator = m_unidentifiedInboundFds.begin();
         iterator != m_unidentifiedInboundFds.end();) {
        const SocketHandle fd = *iterator;
        auto message = pollFd(fd, true);

        if (message.has_value()) {
            rememberConnection(message->fromNodeId(), fd);
            iterator = m_unidentifiedInboundFds.erase(iterator);
            return message;
        }

        ++iterator;
    }

    return std::nullopt;
}

void TcpTransport::closeAll() {
    for (auto& [peer, fd] : m_connectionsByPeer) {
        (void)peer;
        closeSocket(fd);
    }
    m_connectionsByPeer.clear();

    for (SocketHandle& fd : m_unidentifiedInboundFds) {
        closeSocket(fd);
    }
    m_unidentifiedInboundFds.clear();

    closeSocket(m_listenFd);
}

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

        (void)setNonBlocking(fd);
        m_unidentifiedInboundFds.push_back(fd);
    }

    return TransportResult(
        TransportStatus::SENT,
        "TCP accept cycle completed."
    );
}

std::optional<TransportMessage> TcpTransport::pollFd(
    SocketHandle fd,
    bool unidentified
) {
    (void)unidentified;

    if (fd == INVALID_FD || !socketHasReadableData(fd)) {
        return std::nullopt;
    }

    IoctlAvailableBytes availableBytes = 0;
    if (socketAvailableBytes(fd, availableBytes) != 0 || availableBytes < 4) {
        return std::nullopt;
    }

    unsigned char header[4] = {0, 0, 0, 0};
    const int peeked = ::recv(
        static_cast<NativeSocket>(fd),
        reinterpret_cast<char*>(header),
        static_cast<int>(sizeof(header)),
        MSG_PEEK
    );
    if (peeked <= 0) {
        return std::nullopt;
    }

    if (peeked < static_cast<int>(sizeof(header))) {
        return std::nullopt;
    }

    const std::uint32_t frameSize = decodeU32BigEndian(header);
    if (frameSize == 0 || frameSize > MAX_WIRE_FRAME_BYTES) {
        closeFd(fd);
        return std::nullopt;
    }

    if (availableBytes < static_cast<IoctlAvailableBytes>(sizeof(header) + frameSize)) {
        return std::nullopt;
    }

    unsigned char consumedHeader[4] = {0, 0, 0, 0};
    if (!readAll(fd, consumedHeader, sizeof(consumedHeader))) {
        closeFd(fd);
        return std::nullopt;
    }

    std::vector<unsigned char> frame(frameSize);
    if (!readAll(fd, frame.data(), frame.size())) {
        closeFd(fd);
        return std::nullopt;
    }

    try {
        TransportMessage message =
            TcpTransportFrameCodec::decodeTransportMessage(frame);

        if (message.toNodeId() != m_localNodeId) {
            return std::nullopt;
        }

        return message;
    } catch (...) {
        closeFd(fd);
        return std::nullopt;
    }
}

void TcpTransport::rememberConnection(
    const std::string& remoteNodeId,
    SocketHandle fd
) {
    if (!isSafeNodeId(remoteNodeId) || fd == INVALID_FD) {
        return;
    }

    const auto existing = m_connectionsByPeer.find(remoteNodeId);
    if (existing != m_connectionsByPeer.end() && existing->second != fd) {
        SocketHandle oldFd = existing->second;
        closeSocket(oldFd);
    }

    m_connectionsByPeer[remoteNodeId] = fd;
}

void TcpTransport::closeFd(SocketHandle fd) {
    if (fd == INVALID_FD) {
        return;
    }

    for (auto iterator = m_connectionsByPeer.begin();
         iterator != m_connectionsByPeer.end();) {
        if (iterator->second == fd) {
            SocketHandle socketFd = iterator->second;
            closeSocket(socketFd);
            iterator = m_connectionsByPeer.erase(iterator);
        } else {
            ++iterator;
        }
    }

    for (auto iterator = m_unidentifiedInboundFds.begin();
         iterator != m_unidentifiedInboundFds.end();) {
        if (*iterator == fd) {
            SocketHandle socketFd = *iterator;
            closeSocket(socketFd);
            iterator = m_unidentifiedInboundFds.erase(iterator);
        } else {
            ++iterator;
        }
    }
}

void TcpTransport::closePeerConnection(
    const std::string& remoteNodeId
) {
    const auto found = m_connectionsByPeer.find(remoteNodeId);
    if (found == m_connectionsByPeer.end()) {
        return;
    }

    SocketHandle fd = found->second;
    closeSocket(fd);
    m_connectionsByPeer.erase(found);
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
